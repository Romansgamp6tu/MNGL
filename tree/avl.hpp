#pragma once

#include "rotatable_binary_tree.hpp"
#include "tree_serialization.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <class T, class Compare = std::less<T>>
// AVL хранит высоту в metadata и поддерживает разницу высот не больше единицы.
class avl : public rotatable_binary_tree<T, Compare, std::size_t>
{
public:
    using base_type = rotatable_binary_tree<T, Compare, std::size_t>;
    using value_type = T;
    using node_type = typename base_type::node_type;

    avl() = default;

    explicit avl(Compare compare)
        : base_type(std::move(compare))
    {
    }

    static avl from_validated_root(std::unique_ptr<node_type> root)
        requires std::is_default_constructible_v<Compare>
    {
        avl result(Compare{});
        if (root)
        {
            root->set_parent(nullptr);
        }
        result.root_ = std::move(root);
        return result;
    }

    bool insert(T value) override
    {
        // Вставка как в BST; затем обновляем высоты и балансируем путь к корню.
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
                return false;
            }
        }

        auto inserted = std::make_unique<node_type>(std::move(value), std::size_t{ 1 });
        if (!parent)
        {
            this->root_ = std::move(inserted);
        }
        else if (this->less(inserted->value(), parent->value()))
        {
            parent->set_left(std::move(inserted));
        }
        else
        {
            parent->set_right(std::move(inserted));
        }

        rebalance_from(parent);
        return true;
    }

    bool erase(const T& value) override
    {
        node_type* target = this->find(value);
        if (!target)
        {
            return false;
        }

        if (target->left() && target->right())
        {
            // Берём inorder successor: после обмена у него не будет левого ребёнка.
            node_type* successor = target->right();
            while (successor->left())
            {
                successor = successor->left();
            }

            using std::swap;
            swap(target->value(), successor->value());
            target = successor;
        }

        node_type* parent = target->parent();
        const bool was_left_child = parent && parent->left() == target;
        auto removed = this->detach_node(target);

        std::unique_ptr<node_type> child;
        if (removed->left())
        {
            child = removed->detach_left();
        }
        else if (removed->right())
        {
            child = removed->detach_right();
        }

        this->attach_subtree(parent, was_left_child, std::move(child));
        rebalance_from(parent);
        return true;
    }

    std::pair<avl, avl> split(const T& pivot)
        requires std::is_copy_constructible_v<Compare>
    {
        avl left_tree(this->comparator());
        avl right_tree(this->comparator());

        struct split_step
        {
            bool belongs_left;
            std::unique_ptr<node_type> pivot;
            std::unique_ptr<node_type> off_path;
        };

        std::vector<split_step> path;
        path.reserve(height());
        node_type* current = this->root_.get();
        while (current)
        {
            const bool belongs_left = this->less(current->value(), pivot);
            path.push_back({ belongs_left, nullptr, nullptr });
            current = belongs_left ? current->right() : current->left();
        }

        auto remaining = std::move(this->root_);
        for (split_step& step : path)
        {
            step.pivot = std::move(remaining);
            if (step.belongs_left)
            {
                step.off_path = step.pivot->detach_left();
                remaining = step.pivot->detach_right();
            }
            else
            {
                step.off_path = step.pivot->detach_right();
                remaining = step.pivot->detach_left();
            }
        }

        std::unique_ptr<node_type> left_root;
        std::unique_ptr<node_type> right_root;
        for (auto step = path.rbegin(); step != path.rend(); ++step)
        {
            if (step->belongs_left)
            {
                left_root = join_with_root(
                    std::move(step->off_path),
                    std::move(step->pivot),
                    std::move(left_root));
            }
            else
            {
                right_root = join_with_root(
                    std::move(right_root),
                    std::move(step->pivot),
                    std::move(step->off_path));
            }
        }

        left_tree.root_ = std::move(left_root);
        right_tree.root_ = std::move(right_root);
        return { std::move(left_tree), std::move(right_tree) };
    }

    avl& merge(avl&& right)
    {
        if (this == &right)
        {
            throw std::invalid_argument("An AVL tree cannot be merged with itself");
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

        const node_type* maximum = this->root_.get();
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
                "AVL merge requires every left key to precede every right key");
        }

        auto [left_without_maximum, pivot] = detach_max(std::move(this->root_));
        this->root_ = join_with_root(
            std::move(left_without_maximum),
            std::move(pivot),
            std::move(right.root_));
        return *this;
    }

    avl& operator+=(avl&& right)
    {
        return merge(std::move(right));
    }

    std::size_t height() const noexcept
    {
        return node_height(this->root_.get());
    }

    std::string to_string() const override
    {
        return tree_serialization::binary_tree_to_string<T, std::size_t>(
            "avl",
            this->root());
    }

    std::vector<std::uint8_t> to_binary() const override
    {
        return tree_serialization::binary_tree_to_binary<T, std::size_t>(
            tree_serialization::tree_kind::avl,
            this->root());
    }

private:
    static void ensure_compatible_comparators(const Compare& left, const Compare& right)
    {
        if constexpr (requires { static_cast<bool>(left == right); })
        {
            if (!(left == right))
            {
                throw std::invalid_argument("AVL merge requires equivalent comparators");
            }
        }
    }

    static std::size_t node_height(const node_type* node) noexcept
    {
        return node ? node->metadata() : 0;
    }

    static int balance_factor(const node_type* node) noexcept
    {
        return static_cast<int>(node_height(node->left()))
            - static_cast<int>(node_height(node->right()));
    }

    static void update_height(node_type* node) noexcept
    {
        // Пустой ребёнок имеет высоту 0, лист — высоту 1.
        node->metadata() = 1 + std::max(
            node_height(node->left()),
            node_height(node->right()));
    }

    node_type* rotate_left_and_update(node_type* pivot)
    {
        node_type* promoted = base_type::rotate_left(pivot);
        update_height(pivot);
        update_height(promoted);
        return promoted;
    }

    node_type* rotate_right_and_update(node_type* pivot)
    {
        node_type* promoted = base_type::rotate_right(pivot);
        update_height(pivot);
        update_height(promoted);
        return promoted;
    }

    node_type* rebalance(node_type* node)
    {
        update_height(node);
        const int balance = balance_factor(node);

        if (balance > 1)
        {
            // Левый-тяжёлый случай: LR сначала выравниваем левым поворотом.
            if (balance_factor(node->left()) < 0)
            {
                rotate_left_and_update(node->left());
            }
            return rotate_right_and_update(node);
        }

        if (balance < -1)
        {
            // Правый-тяжёлый случай: RL сначала выравниваем правым поворотом.
            if (balance_factor(node->right()) > 0)
            {
                rotate_right_and_update(node->right());
            }
            return rotate_left_and_update(node);
        }

        return node;
    }

    void rebalance_from(node_type* node)
    {
        // После каждого поворота продолжаем от нового корня поддерева вверх.
        while (node)
        {
            node_type* subtree_root = rebalance(node);
            node = subtree_root->parent();
        }
    }

    static std::unique_ptr<node_type> rotate_left_owned(std::unique_ptr<node_type> pivot)
    {
        auto promoted = pivot->detach_right();
        auto middle = promoted->detach_left();
        pivot->set_right(std::move(middle));
        promoted->set_left(std::move(pivot));
        update_height(promoted->left());
        update_height(promoted.get());
        return promoted;
    }

    static std::unique_ptr<node_type> rotate_right_owned(std::unique_ptr<node_type> pivot)
    {
        auto promoted = pivot->detach_left();
        auto middle = promoted->detach_right();
        pivot->set_left(std::move(middle));
        promoted->set_right(std::move(pivot));
        update_height(promoted->right());
        update_height(promoted.get());
        return promoted;
    }

    static std::unique_ptr<node_type> rebalance_owned(std::unique_ptr<node_type> node)
    {
        update_height(node.get());
        const int balance = balance_factor(node.get());

        if (balance > 1)
        {
            if (balance_factor(node->left()) < 0)
            {
                auto left = node->detach_left();
                node->set_left(rotate_left_owned(std::move(left)));
            }

            return rotate_right_owned(std::move(node));
        }

        if (balance < -1)
        {
            if (balance_factor(node->right()) > 0)
            {
                auto right = node->detach_right();
                node->set_right(rotate_right_owned(std::move(right)));
            }

            return rotate_left_owned(std::move(node));
        }

        return node;
    }

    static std::unique_ptr<node_type> join_with_root(
        std::unique_ptr<node_type> left,
        std::unique_ptr<node_type> pivot,
        std::unique_ptr<node_type> right)
    {
        if (node_height(left.get()) > node_height(right.get()) + 1)
        {
            auto right_branch = left->detach_right();
            left->set_right(join_with_root(
                std::move(right_branch),
                std::move(pivot),
                std::move(right)));
            return rebalance_owned(std::move(left));
        }

        if (node_height(right.get()) > node_height(left.get()) + 1)
        {
            auto left_branch = right->detach_left();
            right->set_left(join_with_root(
                std::move(left),
                std::move(pivot),
                std::move(left_branch)));
            return rebalance_owned(std::move(right));
        }

        pivot->set_left(std::move(left));
        pivot->set_right(std::move(right));
        update_height(pivot.get());
        return pivot;
    }

    static std::pair<std::unique_ptr<node_type>, std::unique_ptr<node_type>> detach_max(
        std::unique_ptr<node_type> tree)
    {
        if (!tree->right())
        {
            auto maximum = std::move(tree);
            auto replacement = maximum->detach_left();
            return { std::move(replacement), std::move(maximum) };
        }

        auto right = tree->detach_right();
        auto [new_right, maximum] = detach_max(std::move(right));
        tree->set_right(std::move(new_right));
        return { rebalance_owned(std::move(tree)), std::move(maximum) };
    }
};
