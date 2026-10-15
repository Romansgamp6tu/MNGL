#pragma once

#include "search_index.hpp"
#include "tree_serialization.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <class T, class Compare = std::less<T>>
// B+-дерево хранит значения в листьях, а во внутренних узлах — их разделители.
class b_plus_tree : public search_index<T>
{
public:
    static_assert(
        std::is_copy_constructible_v<T>,
        "B+ tree copies leaf minimum keys into internal separators");
    static_assert(
        std::is_move_constructible_v<T> && std::is_move_assignable_v<T>,
        "B+ tree values must be movable");

    using value_type = T;
    using size_type = std::size_t;
    using visitor_type = typename search_index<T>::visitor_type;

    static constexpr size_type default_max_children = 8;

    class node_type
    {
    public:
        bool is_leaf() const noexcept
        {
            return is_leaf_;
        }

        const std::vector<T>& keys() const noexcept
        {
            return keys_;
        }

        size_type child_count() const noexcept
        {
            return children_.size();
        }

        const node_type* child(size_type index) const
        {
            return children_.at(index).get();
        }

        const node_type* parent() const noexcept
        {
            return parent_;
        }

        const node_type* previous_leaf() const noexcept
        {
            return previous_leaf_;
        }

        const node_type* next_leaf() const noexcept
        {
            return next_leaf_;
        }

    private:
        friend class b_plus_tree;

        explicit node_type(bool is_leaf)
            : is_leaf_(is_leaf)
        {
        }

        bool is_leaf_;
        std::vector<T> keys_;
        std::vector<std::unique_ptr<node_type>> children_;
        node_type* parent_ = nullptr;
        node_type* previous_leaf_ = nullptr;
        node_type* next_leaf_ = nullptr;
    };

    b_plus_tree()
        requires std::default_initializable<Compare> = default;

    explicit b_plus_tree(size_type max_children)
        requires std::default_initializable<Compare>
        : max_children_(validate_fanout(max_children))
    {
    }

    explicit b_plus_tree(Compare compare)
        : compare_(std::move(compare))
    {
    }

    b_plus_tree(size_type max_children, Compare compare)
        : max_children_(validate_fanout(max_children))
        , compare_(std::move(compare))
    {
    }

    b_plus_tree(Compare compare, size_type max_children)
        : b_plus_tree(max_children, std::move(compare))
    {
    }

    b_plus_tree(const b_plus_tree&) = delete;
    b_plus_tree& operator=(const b_plus_tree&) = delete;

    b_plus_tree(b_plus_tree&& other)
        noexcept(std::is_nothrow_move_constructible_v<Compare>)
        : max_children_(other.max_children_)
        , compare_(std::move(other.compare_))
        , root_(std::move(other.root_))
        , first_leaf_(std::exchange(other.first_leaf_, nullptr))
        , last_leaf_(std::exchange(other.last_leaf_, nullptr))
        , size_(std::exchange(other.size_, 0))
    {
    }

    b_plus_tree& operator=(b_plus_tree&& other)
        noexcept(std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other)
        {
            clear();
            max_children_ = other.max_children_;
            compare_ = std::move(other.compare_);
            root_ = std::move(other.root_);
            first_leaf_ = std::exchange(other.first_leaf_, nullptr);
            last_leaf_ = std::exchange(other.last_leaf_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }

        return *this;
    }

    ~b_plus_tree() override = default;

    bool insert(T value) override
    {
        if (!root_)
        {
            root_ = make_node(true);
            root_->keys_.push_back(std::move(value));
            first_leaf_ = root_.get();
            last_leaf_ = root_.get();
            ++size_;
            return true;
        }

        node_type* leaf = find_leaf(value);
        auto position = std::lower_bound(
            leaf->keys_.begin(),
            leaf->keys_.end(),
            value,
            compare_);

        if (position != leaf->keys_.end() && equivalent(*position, value))
        {
            return false;
        }

        leaf->keys_.insert(position, std::move(value));
        ++size_;

        if (leaf->keys_.size() > max_leaf_keys())
        {
            split_leaf(leaf);
        }
        else
        {
            refresh_upwards(leaf->parent_);
        }

        return true;
    }

    bool erase(const T& value) override
    {
        node_type* leaf = find_leaf(value);
        if (!leaf)
        {
            return false;
        }

        auto position = std::lower_bound(
            leaf->keys_.begin(),
            leaf->keys_.end(),
            value,
            compare_);

        if (position == leaf->keys_.end() || !equivalent(*position, value))
        {
            return false;
        }

        leaf->keys_.erase(position);
        --size_;

        if (leaf == root_.get())
        {
            if (leaf->keys_.empty())
            {
                clear();
            }

            return true;
        }

        if (leaf->keys_.size() < min_leaf_keys())
        {
            rebalance_leaf(leaf);
        }
        else
        {
            refresh_upwards(leaf->parent_);
        }

        return true;
    }

    const T* lookup(const T& value) override
    {
        return std::as_const(*this).lookup(value);
    }

    const T* lookup(const T& value) const override
    {
        const node_type* leaf = find_leaf(value);
        if (!leaf)
        {
            return nullptr;
        }

        auto position = std::lower_bound(
            leaf->keys_.begin(),
            leaf->keys_.end(),
            value,
            compare_);

        return position != leaf->keys_.end() && equivalent(*position, value)
            ? &*position
            : nullptr;
    }

    const T* lower_bound(const T& value) const override
    {
        const node_type* leaf = find_leaf(value);

        while (leaf)
        {
            auto position = std::lower_bound(
                leaf->keys_.begin(),
                leaf->keys_.end(),
                value,
                compare_);

            if (position != leaf->keys_.end())
            {
                return &*position;
            }

            leaf = leaf->next_leaf_;
        }

        return nullptr;
    }

    void for_each(const visitor_type& visitor) const override
    {
        for (const node_type* leaf = first_leaf_; leaf; leaf = leaf->next_leaf_)
        {
            for (const T& key : leaf->keys_)
            {
                visitor(key);
            }
        }
    }

    void for_each_range(
        const T& lower,
        const T& upper,
        const visitor_type& visitor) const override
    {
        const node_type* leaf = find_leaf(lower);

        while (leaf)
        {
            auto position = std::lower_bound(
                leaf->keys_.begin(),
                leaf->keys_.end(),
                lower,
                compare_);

            for (; position != leaf->keys_.end(); ++position)
            {
                if (!compare_(*position, upper))
                {
                    return;
                }

                visitor(*position);
            }

            leaf = leaf->next_leaf_;
        }
    }

    std::string to_string() const override
    {
        std::ostringstream output;
        output << "b_plus_tree[fanout=" << max_children_ << "]{";
        tree_serialization::write_b_plus_text_node(output, root_.get());
        output << '}';
        return output.str();
    }

    std::vector<std::uint8_t> to_binary() const override
    {
        if constexpr (!std::is_trivially_copyable_v<T>)
        {
            throw std::logic_error("Binary tree export requires trivially copyable values");
        }
        else
        {
            tree_serialization::binary_writer output(
                tree_serialization::tree_kind::b_plus,
                typeid(T).name(),
                sizeof(T));
            output.write_u64(static_cast<std::uint64_t>(max_children_));
            output.write_u64(static_cast<std::uint64_t>(size_));
            tree_serialization::write_b_plus_binary_node(output, root_.get());
            return std::move(output).take();
        }
    }

    bool empty() const noexcept
    {
        return size_ == 0;
    }

    size_type size() const noexcept
    {
        return size_;
    }

    size_type max_children() const noexcept
    {
        return max_children_;
    }

    size_type max_leaf_keys() const noexcept
    {
        return max_children_ - 1;
    }

    node_type* root() noexcept
    {
        return root_.get();
    }

    const node_type* root() const noexcept
    {
        return root_.get();
    }

    void clear() noexcept
    {
        root_.reset();
        first_leaf_ = nullptr;
        last_leaf_ = nullptr;
        size_ = 0;
    }

private:
    static size_type validate_fanout(size_type max_children)
    {
        if (max_children < 3)
        {
            throw std::invalid_argument("B+ tree fanout must be at least 3");
        }

        return max_children;
    }

    static std::unique_ptr<node_type> make_node(bool is_leaf)
    {
        return std::unique_ptr<node_type>(new node_type(is_leaf));
    }

    static size_type min_leaf_keys_for(size_type max_children) noexcept
    {
        return max_children / 2;
    }

    size_type min_leaf_keys() const noexcept
    {
        return min_leaf_keys_for(max_children_);
    }

    size_type min_internal_children() const noexcept
    {
        return max_children_ / 2 + max_children_ % 2;
    }

    bool equivalent(const T& left, const T& right) const
    {
        return !compare_(left, right) && !compare_(right, left);
    }

    size_type child_index(const node_type* node, const T& value) const
    {
        size_type index = 0;
        while (index < node->keys_.size() && !compare_(value, node->keys_[index]))
        {
            ++index;
        }

        return index;
    }

    node_type* find_leaf(const T& value)
    {
        return const_cast<node_type*>(std::as_const(*this).find_leaf(value));
    }

    const node_type* find_leaf(const T& value) const
    {
        const node_type* current = root_.get();
        while (current && !current->is_leaf_)
        {
            current = current->children_[child_index(current, value)].get();
        }

        return current;
    }

    static const T& minimum_key(const node_type* node)
    {
        while (node && !node->is_leaf_)
        {
            if (node->children_.empty())
            {
                throw std::logic_error("B+ tree internal node has no children");
            }

            node = node->children_.front().get();
        }

        if (!node || node->keys_.empty())
        {
            throw std::logic_error("B+ tree subtree has no minimum key");
        }

        return node->keys_.front();
    }

    static void refresh_keys(node_type* node)
    {
        std::vector<T> separators;
        if (node->children_.size() > 1)
        {
            separators.reserve(node->children_.size() - 1);
            for (size_type i = 1; i < node->children_.size(); ++i)
            {
                separators.push_back(minimum_key(node->children_[i].get()));
            }
        }

        node->keys_.swap(separators);
    }

    void refresh_upwards(node_type* node)
    {
        while (node)
        {
            if (!node->is_leaf_)
            {
                refresh_keys(node);
            }

            node = node->parent_;
        }
    }

    static size_type child_position(const node_type* parent, const node_type* child)
    {
        for (size_type i = 0; i < parent->children_.size(); ++i)
        {
            if (parent->children_[i].get() == child)
            {
                return i;
            }
        }

        throw std::logic_error("B+ tree node is not attached to its parent");
    }

    static std::unique_ptr<node_type> remove_child(node_type* parent, size_type index)
    {
        std::unique_ptr<node_type> removed = std::move(parent->children_[index]);
        parent->children_.erase(parent->children_.begin() + static_cast<std::ptrdiff_t>(index));
        removed->parent_ = nullptr;
        return removed;
    }

    void split_leaf(node_type* leaf)
    {
        auto right = make_node(true);
        const size_type split_index = leaf->keys_.size() / 2;
        right->keys_.insert(
            right->keys_.end(),
            leaf->keys_.begin() + static_cast<std::ptrdiff_t>(split_index),
            leaf->keys_.end());
        leaf->keys_.erase(
            leaf->keys_.begin() + static_cast<std::ptrdiff_t>(split_index),
            leaf->keys_.end());

        right->previous_leaf_ = leaf;
        right->next_leaf_ = leaf->next_leaf_;
        if (leaf->next_leaf_)
        {
            leaf->next_leaf_->previous_leaf_ = right.get();
        }
        else
        {
            last_leaf_ = right.get();
        }

        leaf->next_leaf_ = right.get();
        insert_right_sibling(leaf, std::move(right));
    }

    void split_internal(node_type* node)
    {
        auto right = make_node(false);
        const size_type split_index = node->children_.size() / 2;
        right->children_.reserve(node->children_.size() - split_index);

        for (size_type i = split_index; i < node->children_.size(); ++i)
        {
            right->children_.push_back(std::move(node->children_[i]));
            right->children_.back()->parent_ = right.get();
        }

        node->children_.erase(
            node->children_.begin() + static_cast<std::ptrdiff_t>(split_index),
            node->children_.end());

        refresh_keys(node);
        refresh_keys(right.get());
        insert_right_sibling(node, std::move(right));
    }

    void insert_right_sibling(node_type* left, std::unique_ptr<node_type> right)
    {
        node_type* parent = left->parent_;
        if (!parent)
        {
            if (root_.get() != left)
            {
                throw std::logic_error("Cannot split a detached B+ tree node");
            }

            auto new_root = make_node(false);
            auto old_root = std::move(root_);
            old_root->parent_ = new_root.get();
            right->parent_ = new_root.get();
            new_root->children_.push_back(std::move(old_root));
            new_root->children_.push_back(std::move(right));
            refresh_keys(new_root.get());
            root_ = std::move(new_root);
            return;
        }

        const size_type index = child_position(parent, left);
        right->parent_ = parent;
        parent->children_.insert(
            parent->children_.begin() + static_cast<std::ptrdiff_t>(index + 1),
            std::move(right));
        refresh_upwards(parent);

        if (parent->children_.size() > max_children_)
        {
            split_internal(parent);
        }
    }

    void rebalance_leaf(node_type* leaf)
    {
        node_type* parent = leaf->parent_;
        const size_type index = child_position(parent, leaf);
        node_type* left = index > 0 ? parent->children_[index - 1].get() : nullptr;
        node_type* right = index + 1 < parent->children_.size()
            ? parent->children_[index + 1].get()
            : nullptr;

        if (left && left->keys_.size() > min_leaf_keys())
        {
            leaf->keys_.insert(leaf->keys_.begin(), left->keys_.back());
            left->keys_.pop_back();
            refresh_upwards(parent);
            return;
        }

        if (right && right->keys_.size() > min_leaf_keys())
        {
            leaf->keys_.push_back(right->keys_.front());
            right->keys_.erase(right->keys_.begin());
            refresh_upwards(parent);
            return;
        }

        if (left)
        {
            left->keys_.insert(left->keys_.end(), leaf->keys_.begin(), leaf->keys_.end());
            left->next_leaf_ = leaf->next_leaf_;
            if (leaf->next_leaf_)
            {
                leaf->next_leaf_->previous_leaf_ = left;
            }
            else
            {
                last_leaf_ = left;
            }

            auto removed = remove_child(parent, index);
            rebalance_internal(parent);
            return;
        }

        if (right)
        {
            leaf->keys_.insert(leaf->keys_.end(), right->keys_.begin(), right->keys_.end());
            leaf->next_leaf_ = right->next_leaf_;
            if (right->next_leaf_)
            {
                right->next_leaf_->previous_leaf_ = leaf;
            }
            else
            {
                last_leaf_ = leaf;
            }

            auto removed = remove_child(parent, index + 1);
            rebalance_internal(parent);
            return;
        }

        throw std::logic_error("Underfull B+ tree leaf has no sibling");
    }

    void rebalance_internal(node_type* node)
    {
        if (node == root_.get())
        {
            if (node->children_.empty())
            {
                throw std::logic_error("B+ tree root has no children");
            }
            else if (node->children_.size() == 1)
            {
                auto old_root = std::move(root_);
                root_ = std::move(old_root->children_.front());
                root_->parent_ = nullptr;
            }
            else
            {
                refresh_keys(node);
            }

            return;
        }

        if (node->children_.size() >= min_internal_children())
        {
            refresh_upwards(node);
            return;
        }

        node_type* parent = node->parent_;
        const size_type index = child_position(parent, node);
        node_type* left = index > 0 ? parent->children_[index - 1].get() : nullptr;
        node_type* right = index + 1 < parent->children_.size()
            ? parent->children_[index + 1].get()
            : nullptr;

        if (left && left->children_.size() > min_internal_children())
        {
            node->children_.reserve(node->children_.size() + 1);
            auto borrowed = std::move(left->children_.back());
            left->children_.pop_back();
            borrowed->parent_ = node;
            node->children_.insert(node->children_.begin(), std::move(borrowed));
            refresh_keys(left);
            refresh_keys(node);
            refresh_upwards(parent);
            return;
        }

        if (right && right->children_.size() > min_internal_children())
        {
            node->children_.reserve(node->children_.size() + 1);
            auto borrowed = std::move(right->children_.front());
            right->children_.erase(right->children_.begin());
            borrowed->parent_ = node;
            node->children_.push_back(std::move(borrowed));
            refresh_keys(right);
            refresh_keys(node);
            refresh_upwards(parent);
            return;
        }

        if (left)
        {
            left->children_.reserve(left->children_.size() + node->children_.size());
            for (auto& child : node->children_)
            {
                child->parent_ = left;
                left->children_.push_back(std::move(child));
            }
            node->children_.clear();
            refresh_keys(left);

            auto removed = remove_child(parent, index);
            rebalance_internal(parent);
            return;
        }

        if (right)
        {
            node->children_.reserve(node->children_.size() + right->children_.size());
            for (auto& child : right->children_)
            {
                child->parent_ = node;
                node->children_.push_back(std::move(child));
            }
            right->children_.clear();
            refresh_keys(node);

            auto removed = remove_child(parent, index + 1);
            rebalance_internal(parent);
            return;
        }

        throw std::logic_error("Underfull B+ tree node has no sibling");
    }

    size_type max_children_ = default_max_children;
    [[no_unique_address]] Compare compare_{};
    std::unique_ptr<node_type> root_;
    node_type* first_leaf_ = nullptr;
    node_type* last_leaf_ = nullptr;
    size_type size_ = 0;
};

template <class T, class Compare = std::less<T>>
using bplus_tree = b_plus_tree<T, Compare>;
