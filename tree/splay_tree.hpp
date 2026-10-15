#pragma once

#include "rotatable_binary_tree.hpp"
#include "tree_serialization.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T, class Compare = std::less<T>>
// Splay-дерево поднимает часто используемые узлы к корню поворотами.
class splay_tree : public rotatable_binary_tree<T, Compare>
{
public:
    using base_type = rotatable_binary_tree<T, Compare>;
    using value_type = T;
    using node_type = typename base_type::node_type;

    splay_tree() = default;

    explicit splay_tree(Compare compare)
        : base_type(std::move(compare))
    {
    }

    splay_tree(const splay_tree&) = delete;
    splay_tree& operator=(const splay_tree&) = delete;
    splay_tree(splay_tree&&) = default;
    splay_tree& operator=(splay_tree&&) = default;

    static splay_tree from_validated_root(std::unique_ptr<node_type> root)
        requires std::is_default_constructible_v<Compare>
    {
        splay_tree result(Compare{});
        if (root)
        {
            root->set_parent(nullptr);
        }
        result.root_ = std::move(root);
        return result;
    }

    bool insert(T value) override
    {
        node_type* parent = nullptr;
        node_type* current = this->root_.get();

        while (current)
        {
            parent = current;
            if (this->less(value, current->value()))
            {
                current = current->left();
            }
            else if (this->less(current->value(), value))
            {
                current = current->right();
            }
            else
            {
                splay(current);
                return false;
            }
        }

        auto inserted = std::make_unique<node_type>(std::move(value));
        node_type* inserted_node = inserted.get();

        if (!parent)
        {
            this->root_ = std::move(inserted);
        }
        else if (this->less(inserted_node->value(), parent->value()))
        {
            parent->set_left(std::move(inserted));
        }
        else
        {
            parent->set_right(std::move(inserted));
        }

        splay(inserted_node);
        return true;
    }

    bool erase(const T& value) override
    {
        node_type* target = find(value);
        if (!target)
        {
            return false;
        }

        auto removed = this->detach_node(target);
        auto left_subtree = removed->detach_left();
        auto right_subtree = removed->detach_right();

        if (!left_subtree)
        {
            this->root_ = std::move(right_subtree);
            return true;
        }

        this->root_ = std::move(left_subtree);
        node_type* maximum = this->root_.get();
        while (maximum->right())
        {
            maximum = maximum->right();
        }

        splay(maximum);
        this->root_->set_right(std::move(right_subtree));
        return true;
    }

    std::pair<splay_tree, splay_tree> split(const T& pivot)
        requires std::is_copy_constructible_v<Compare>
    {
        splay_tree left_tree(this->comparator());
        splay_tree right_tree(this->comparator());
        node_type* current = this->root_.get();
        node_type* last = nullptr;
        node_type* first_right = nullptr;

        while (current)
        {
            last = current;
            if (!this->less(current->value(), pivot))
            {
                first_right = current;
                current = current->left();
            }
            else
            {
                current = current->right();
            }
        }

        if (first_right)
        {
            splay(first_right);
            auto right_root = std::move(this->root_);
            left_tree.root_ = right_root->detach_left();
            right_tree.root_ = std::move(right_root);
        }
        else if (last)
        {
            splay(last);
            left_tree.root_ = std::move(this->root_);
        }

        return { std::move(left_tree), std::move(right_tree) };
    }

    splay_tree& merge(splay_tree&& right)
    {
        if (this == &right)
        {
            throw std::invalid_argument("A Splay tree cannot be merged with itself");
        }
        if (right.empty())
        {
            return *this;
        }

        ensure_compatible_comparators(this->comparator(), right.comparator());
        if (this->empty())
        {
            this->root_ = std::move(right.root_);
            return *this;
        }

        node_type* maximum = this->root_.get();
        while (maximum->right())
        {
            maximum = maximum->right();
        }
        const node_type* minimum_right = right.root_.get();
        while (minimum_right->left())
        {
            minimum_right = minimum_right->left();
        }
        if (!this->less(maximum->value(), minimum_right->value()))
        {
            throw std::invalid_argument(
                "Splay merge requires every left key to precede every right key");
        }

        splay(maximum);
        this->root_->set_right(std::move(right.root_));
        return *this;
    }

    node_type* find(const T& value)
    {
        node_type* current = this->root_.get();
        node_type* last = nullptr;

        while (current)
        {
            last = current;
            if (this->less(value, current->value()))
            {
                current = current->left();
            }
            else if (this->less(current->value(), value))
            {
                current = current->right();
            }
            else
            {
                splay(current);
                return current;
            }
        }

        if (last)
        {
            splay(last);
        }

        return nullptr;
    }

    const node_type* find(const T& value) const
    {
        return base_type::find(value);
    }

    const T* lookup(const T& value) override
    {
        node_type* node = find(value);
        return node ? &node->value() : nullptr;
    }

    const T* lookup(const T& value) const override
    {
        return base_type::lookup(value);
    }

    std::string to_string() const override
    {
        return tree_serialization::binary_tree_to_string<T, void>(
            "splay",
            this->root());
    }

    std::vector<std::uint8_t> to_binary() const override
    {
        return tree_serialization::binary_tree_to_binary<T, void>(
            tree_serialization::tree_kind::splay,
            this->root());
    }

private:
    static void ensure_compatible_comparators(
        const Compare& left,
        const Compare& right)
    {
        if constexpr (requires { static_cast<bool>(left == right); })
        {
            if (!(left == right))
            {
                throw std::invalid_argument(
                    "Splay merge requires equivalent comparators");
            }
        }
    }

    void splay(node_type* node)
    {
        while (node->parent())
        {
            node_type* parent = node->parent();
            node_type* grandparent = parent->parent();

            if (!grandparent)
            {
                if (parent->left() == node)
                {
                    this->rotate_right(parent);
                }
                else
                {
                    this->rotate_left(parent);
                }
            }
            else if (grandparent->left() == parent && parent->left() == node)
            {
                this->rotate_right(grandparent);
                this->rotate_right(parent);
            }
            else if (grandparent->right() == parent && parent->right() == node)
            {
                this->rotate_left(grandparent);
                this->rotate_left(parent);
            }
            else if (grandparent->left() == parent)
            {
                this->rotate_left(parent);
                this->rotate_right(grandparent);
            }
            else
            {
                this->rotate_right(parent);
                this->rotate_left(grandparent);
            }
        }
    }
};

template <class T, class Compare = std::less<T>>
using splay = splay_tree<T, Compare>;
