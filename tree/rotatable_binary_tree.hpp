#pragma once

#include "search_tree.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <class T, class Compare = std::less<T>, class Meta = void>
// Общая база для поисковых деревьев, которым нужны повороты поддеревьев.
class rotatable_binary_tree : public search_tree<T, Meta>
{
public:
    using base_type = search_tree<T, Meta>;
    using value_type = T;
    using node_type = typename base_type::node_type;

    rotatable_binary_tree() = default;

    explicit rotatable_binary_tree(Compare compare)
        : compare_(std::move(compare))
    {
    }

    rotatable_binary_tree(const rotatable_binary_tree&) = delete;
    rotatable_binary_tree& operator=(const rotatable_binary_tree&) = delete;
    rotatable_binary_tree(rotatable_binary_tree&&)
        noexcept(std::is_nothrow_move_constructible_v<Compare>) = default;

    rotatable_binary_tree& operator=(rotatable_binary_tree&& other)
        noexcept(std::is_nothrow_move_assignable_v<Compare>)
    {
        if (this != &other)
        {
            clear();
            compare_ = std::move(other.compare_);
            root_ = std::move(other.root_);
        }

        return *this;
    }

    ~rotatable_binary_tree() override
    {
        clear();
    }

    bool contains(const T& value) const override
    {
        return find(value) != nullptr;
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
        // Разворачиваем дерево в цепочку поворотами, чтобы не удалять рекурсивно.
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

    const Compare& comparator() const noexcept
    {
        return compare_;
    }

    node_type* rotate_left(node_type* pivot)
    {
        if (!pivot)
        {
            throw std::invalid_argument("Cannot rotate a null tree node");
        }
        if (!pivot->right())
        {
            throw std::logic_error("Left rotation requires a right child");
        }

        // Правый ребёнок поднимается, а его левое поддерево становится правым у pivot.
        node_type* parent = pivot->parent();
        const bool was_left_child = parent && parent->left() == pivot;

        auto pivot_owner = detach_node(pivot);
        auto promoted = pivot_owner->detach_right();
        auto middle = promoted->detach_left();

        pivot_owner->set_right(std::move(middle));
        promoted->set_left(std::move(pivot_owner));
        attach_subtree(parent, was_left_child, std::move(promoted));

        return parent
            ? (was_left_child ? parent->left() : parent->right())
            : root_.get();
    }

    node_type* rotate_right(node_type* pivot)
    {
        if (!pivot)
        {
            throw std::invalid_argument("Cannot rotate a null tree node");
        }
        if (!pivot->left())
        {
            throw std::logic_error("Right rotation requires a left child");
        }

        // Левый ребёнок поднимается, а его правое поддерево становится левым у pivot.
        node_type* parent = pivot->parent();
        const bool was_left_child = parent && parent->left() == pivot;

        auto pivot_owner = detach_node(pivot);
        auto promoted = pivot_owner->detach_left();
        auto middle = promoted->detach_right();

        pivot_owner->set_left(std::move(middle));
        promoted->set_right(std::move(pivot_owner));
        attach_subtree(parent, was_left_child, std::move(promoted));

        return parent
            ? (was_left_child ? parent->left() : parent->right())
            : root_.get();
    }

    std::unique_ptr<node_type> detach_node(node_type* node)
    {
        // Забираем владение узлом у родителя либо у корня дерева.
        if (!node)
        {
            throw std::invalid_argument("Cannot detach a null tree node");
        }

        node_type* parent = node->parent();
        if (!parent)
        {
            if (root_.get() != node)
            {
                throw std::logic_error("Tree node is not attached to this tree");
            }

            return std::move(root_);
        }

        if (parent->left() == node)
        {
            return parent->detach_left();
        }
        if (parent->right() == node)
        {
            return parent->detach_right();
        }

        throw std::logic_error("Tree node is detached from its parent");
    }

    void attach_subtree(
        node_type* parent,
        bool as_left_child,
        std::unique_ptr<node_type> subtree) noexcept
    {
        // Подключаем поддерево на прежнее место и синхронизируем parent.
        if (!parent)
        {
            if (subtree)
            {
                subtree->set_parent(nullptr);
            }
            root_ = std::move(subtree);
        }
        else if (as_left_child)
        {
            parent->set_left(std::move(subtree));
        }
        else
        {
            parent->set_right(std::move(subtree));
        }
    }

private:
    [[no_unique_address]] Compare compare_{};

protected:
    std::unique_ptr<node_type> root_;
};
