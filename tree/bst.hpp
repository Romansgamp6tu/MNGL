#pragma once

#include "search_tree.hpp"
#include "tree_serialization.hpp"

#include <functional>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <class T, class Compare = std::less<T>, class Meta = void>
// Обычное BST без балансировки: дубликаты не вставляются.
class bst : public search_tree<T, Meta>
{
public:
    using base_type = search_tree<T, Meta>;
    using value_type = T;
    using node_type = typename base_type::node_type;

    bst() = default;

    explicit bst(Compare compare)
        : compare_(std::move(compare))
    {
    }

    static bst from_validated_root(std::unique_ptr<node_type> root)
        requires std::is_default_constructible_v<Compare>
    {
        bst result(Compare{});
        if (root)
        {
            root->set_parent(nullptr);
        }
        result.root_ = std::move(root);
        return result;
    }

    bst(const bst&) = delete;
    bst& operator=(const bst&) = delete;
    bst(bst&&) noexcept(std::is_nothrow_move_constructible_v<Compare>) = default;

    bst& operator=(bst&& other) noexcept(std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other)
        {
            clear();
            compare_ = std::move(other.compare_);
            root_ = std::move(other.root_);
        }

        return *this;
    }

    ~bst() override
    {
        clear();
    }

    bool insert(T value) override
    {
        if (!root_)
        {
            root_ = std::make_unique<node_type>(std::move(value));
            return true;
        }

        // Спускаемся по компаратору до свободного места.
        node_type* current = root_.get();

        while (true)
        {
            if (compare_(value, current->value()))
            {
                if (!current->left())
                {
                    current->set_left(std::make_unique<node_type>(std::move(value)));
                    return true;
                }

                current = current->left();
            }
            else if (compare_(current->value(), value))
            {
                if (!current->right())
                {
                    current->set_right(std::make_unique<node_type>(std::move(value)));
                    return true;
                }

                current = current->right();
            }
            else
            {
                return false;
            }
        }
    }

    bool erase(const T& value) override
    {
        node_type* target = find(value);
        if (!target)
        {
            return false;
        }

        if (target->left() && target->right())
        {
            // Заменяем значение следующим по порядку узлом, затем удаляем его.
            node_type* successor = target->right();
            while (successor->left())
            {
                successor = successor->left();
            }

            using std::swap;
            swap(target->value(), successor->value());
            remove_node(successor);
        }
        else
        {
            remove_node(target);
        }

        return true;
    }

    std::pair<bst, bst> split(const T& pivot)
        requires std::is_copy_constructible_v<Compare>
    {
        bst left_tree(compare_);
        bst right_tree(compare_);

        std::vector<std::uint8_t> path;
        node_type* current = root_.get();
        while (current)
        {
            const bool belongs_left = compare_(current->value(), pivot);
            path.push_back(belongs_left ? 1 : 0);
            current = belongs_left ? current->right() : current->left();
        }

        auto remaining = std::move(root_);
        node_type* left_tail = nullptr;
        node_type* right_tail = nullptr;

        for (std::uint8_t direction : path)
        {
            auto current_owner = std::move(remaining);
            const bool belongs_left = direction != 0;

            if (belongs_left)
            {
                remaining = current_owner->detach_right();
                if (!left_tree.root_)
                {
                    left_tree.root_ = std::move(current_owner);
                    left_tail = left_tree.root_.get();
                }
                else
                {
                    left_tail->set_right(std::move(current_owner));
                    left_tail = left_tail->right();
                }
            }
            else
            {
                remaining = current_owner->detach_left();
                if (!right_tree.root_)
                {
                    right_tree.root_ = std::move(current_owner);
                    right_tail = right_tree.root_.get();
                }
                else
                {
                    right_tail->set_left(std::move(current_owner));
                    right_tail = right_tail->left();
                }
            }
        }

        return { std::move(left_tree), std::move(right_tree) };
    }

    bst& merge(bst&& right)
    {
        if (this == &right)
        {
            throw std::invalid_argument("A BST cannot be merged with itself");
        }

        if (right.empty())
        {
            return *this;
        }

        ensure_compatible_comparators(compare_, right.compare_);
        if (empty())
        {
            root_ = std::move(right.root_);
            return *this;
        }

        node_type* maximum = root_.get();
        while (maximum->right())
        {
            maximum = maximum->right();
        }

        const node_type* minimum_right = right.root_.get();
        while (minimum_right->left())
        {
            minimum_right = minimum_right->left();
        }

        if (!compare_(maximum->value(), minimum_right->value()))
        {
            throw std::invalid_argument(
                "BST merge requires every left key to precede every right key");
        }

        node_type* parent = maximum->parent();
        const bool was_left_child = parent && parent->left() == maximum;
        auto pivot = detach_node(maximum);
        auto replacement = pivot->detach_left();

        if (!parent)
        {
            root_ = std::move(replacement);
        }
        else if (was_left_child)
        {
            parent->set_left(std::move(replacement));
        }
        else
        {
            parent->set_right(std::move(replacement));
        }

        pivot->set_left(std::move(root_));
        pivot->set_right(std::move(right.root_));
        root_ = std::move(pivot);
        return *this;
    }

    bst& operator+=(bst&& right)
    {
        return merge(std::move(right));
    }

    bool contains(const T& value) const override
    {
        return find(value) != nullptr;
    }

    std::string to_string() const override
    {
        return tree_serialization::binary_tree_to_string<T, Meta>("bst", root_.get());
    }

    std::vector<std::uint8_t> to_binary() const override
    {
        return tree_serialization::binary_tree_to_binary<T, Meta>(
            tree_serialization::tree_kind::bst,
            root_.get());
    }

    node_type* find(const T& value)
    {
        return const_cast<node_type*>(std::as_const(*this).find(value));
    }

    const node_type* find(const T& value) const
    {
        const node_type* current = root_.get();

        while (current)
        {
            if (compare_(value, current->value()))
            {
                current = current->left();
            }
            else if (compare_(current->value(), value))
            {
                current = current->right();
            }
            else
            {
                return current;
            }
        }

        return nullptr;
    }

    bool empty() const noexcept
    {
        return !root_;
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
        // Поворотами превращаем дерево в цепочку и удаляем без рекурсии.
        while (root_)
        {
            if (root_->left())
            {
                auto left = root_->detach_left();
                auto middle = left->detach_right();
                root_->set_left(std::move(middle));
                left->set_right(std::move(root_));
                root_ = std::move(left);
            }
            else
            {
                root_ = root_->detach_right();
            }
        }
    }

protected:
    node_type* root_node() noexcept override
    {
        return root_.get();
    }

    const node_type* root_node() const noexcept override
    {
        return root_.get();
    }

    bool less(const T& left, const T& right) const override
    {
        return compare_(left, right);
    }

private:
    static void ensure_compatible_comparators(const Compare& left, const Compare& right)
    {
        if constexpr (requires { static_cast<bool>(left == right); })
        {
            if (!(left == right))
            {
                throw std::invalid_argument("BST merge requires equivalent comparators");
            }
        }
    }

    std::unique_ptr<node_type> detach_node(node_type* node)
    {
        node_type* parent = node->parent();
        if (!parent)
        {
            return std::move(root_);
        }

        if (parent->left() == node)
        {
            return parent->detach_left();
        }

        return parent->detach_right();
    }

    void remove_node(node_type* node)
    {
        // У удаляемого узла не больше одного ребёнка; подключаем его вместо узла.
        node_type* parent = node->parent();
        const bool was_left_child = parent && parent->left() == node;
        std::unique_ptr<node_type> removed = detach_node(node);

        std::unique_ptr<node_type> child;
        if (removed->left())
        {
            child = removed->detach_left();
        }
        else if (removed->right())
        {
            child = removed->detach_right();
        }

        if (!parent)
        {
            root_ = std::move(child);
        }
        else if (was_left_child)
        {
            parent->set_left(std::move(child));
        }
        else
        {
            parent->set_right(std::move(child));
        }
    }

    [[no_unique_address]] Compare compare_{};
    std::unique_ptr<node_type> root_;
};
