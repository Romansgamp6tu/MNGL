#pragma once

#include "rotatable_binary_tree.hpp"
#include "tree_serialization.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

template <class T, class Compare = std::less<T>>
// Декартово дерево хранит случайный приоритет и поддерживает min-heap инвариант.
class treap : public rotatable_binary_tree<T, Compare, std::uint64_t>
{
public:
    using base_type = rotatable_binary_tree<T, Compare, std::uint64_t>;
    using value_type = T;
    using node_type = typename base_type::node_type;
    using seed_type = std::mt19937_64::result_type;

    treap()
        : treap(Compare{}, random_seed())
    {
    }

    explicit treap(seed_type seed)
        : treap(Compare{}, seed)
    {
    }

    explicit treap(Compare compare)
        : treap(std::move(compare), random_seed())
    {
    }

    treap(Compare compare, std::uint64_t seed)
        : base_type(std::move(compare))
        , random_engine_(seed)
    {
    }

    static treap from_validated_root(std::unique_ptr<node_type> root)
        requires std::is_default_constructible_v<Compare>
    {
        treap result;
        if (root)
        {
            root->set_parent(nullptr);
        }
        result.root_ = std::move(root);
        return result;
    }

    treap(const treap&) = delete;
    treap& operator=(const treap&) = delete;
    treap(treap&&) = default;
    treap& operator=(treap&&) = default;

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
                return false;
            }
        }

        auto inserted = std::make_unique<node_type>(
            std::move(value),
            random_engine_());
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

        while (inserted_node->parent()
            && inserted_node->metadata() < inserted_node->parent()->metadata())
        {
            node_type* node_parent = inserted_node->parent();
            if (node_parent->left() == inserted_node)
            {
                this->rotate_right(node_parent);
            }
            else
            {
                this->rotate_left(node_parent);
            }
        }

        return true;
    }

    bool erase(const T& value) override
    {
        node_type* target = this->find(value);
        if (!target)
        {
            return false;
        }

        // Поворотами опускаем удаляемый узел, не нарушая heap-порядок приоритетов.
        while (target->left() || target->right())
        {
            if (!target->left())
            {
                this->rotate_left(target);
            }
            else if (!target->right())
            {
                this->rotate_right(target);
            }
            else if (target->left()->metadata() < target->right()->metadata())
            {
                this->rotate_right(target);
            }
            else
            {
                this->rotate_left(target);
            }
        }

        auto removed = this->detach_node(target);
        return true;
    }

    std::pair<treap, treap> split(const T& pivot)
        requires std::is_copy_constructible_v<Compare>
    {
        treap left_tree(this->comparator(), random_seed());
        treap right_tree(this->comparator(), random_seed());
        std::vector<std::uint8_t> path;
        const node_type* current = this->root_.get();
        while (current)
        {
            const bool belongs_left = this->less(current->value(), pivot);
            path.push_back(belongs_left ? 1 : 0);
            current = belongs_left ? current->right() : current->left();
        }

        auto remaining = std::move(this->root_);
        node_type* left_tail = nullptr;
        node_type* right_tail = nullptr;
        for (const std::uint8_t direction : path)
        {
            auto current_owner = std::move(remaining);
            if (direction != 0)
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

    treap& merge(treap&& right)
    {
        if (this == &right)
        {
            throw std::invalid_argument("A treap cannot be merged with itself");
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
                "Treap merge requires every left key to precede every right key");
        }

        struct merge_step
        {
            std::unique_ptr<node_type> node;
            bool from_left;
        };

        std::size_t path_size = 0;
        const node_type* left_cursor = this->root_.get();
        const node_type* right_cursor = right.root_.get();
        while (left_cursor && right_cursor)
        {
            ++path_size;
            if (left_cursor->metadata() <= right_cursor->metadata())
            {
                left_cursor = left_cursor->right();
            }
            else
            {
                right_cursor = right_cursor->left();
            }
        }

        std::vector<merge_step> path;
        path.reserve(path_size);
        auto left_root = std::move(this->root_);
        auto right_root = std::move(right.root_);
        while (left_root && right_root)
        {
            if (left_root->metadata() <= right_root->metadata())
            {
                merge_step step{ std::move(left_root), true };
                left_root = step.node->detach_right();
                path.push_back(std::move(step));
            }
            else
            {
                merge_step step{ std::move(right_root), false };
                right_root = step.node->detach_left();
                path.push_back(std::move(step));
            }
        }

        auto merged_root = left_root
            ? std::move(left_root)
            : std::move(right_root);
        for (auto step = path.rbegin(); step != path.rend(); ++step)
        {
            if (step->from_left)
            {
                step->node->set_right(std::move(merged_root));
            }
            else
            {
                step->node->set_left(std::move(merged_root));
            }
            merged_root = std::move(step->node);
        }
        this->root_ = std::move(merged_root);
        return *this;
    }

    std::string to_string() const override
    {
        return tree_serialization::binary_tree_to_string<T, std::uint64_t>(
            "treap",
            this->root());
    }

    std::vector<std::uint8_t> to_binary() const override
    {
        return tree_serialization::binary_tree_to_binary<T, std::uint64_t>(
            tree_serialization::tree_kind::treap,
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
                    "Treap merge requires equivalent comparators");
            }
        }
    }

    static std::uint64_t random_seed()
    {
        return static_cast<std::uint64_t>(std::random_device{}());
    }

    std::mt19937_64 random_engine_;
};

template <class T, class Compare = std::less<T>>
using cartesian_tree = treap<T, Compare>;
