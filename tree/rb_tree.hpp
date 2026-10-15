#pragma once

#include "rotatable_binary_tree.hpp"
#include "tree_serialization.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

enum class rb_color
{
    red,
    black
};

template <class T, class Compare = std::less<T>>
// Красно-чёрное дерево: баланс поддерживается цветами и поворотами.
class rb_tree : public rotatable_binary_tree<T, Compare, rb_color>
{
public:
    using base_type = rotatable_binary_tree<T, Compare, rb_color>;
    using value_type = T;
    using node_type = typename base_type::node_type;

    rb_tree() = default;

    explicit rb_tree(Compare compare)
        : base_type(std::move(compare))
    {
    }

    static rb_tree from_validated_root(std::unique_ptr<node_type> root)
        requires std::is_default_constructible_v<Compare>
    {
        rb_tree result(Compare{});
        if (root)
        {
            root->set_parent(nullptr);
        }
        result.root_ = std::move(root);
        return result;
    }

    bool insert(T value) override
    {
        // Новый узел красный; исправление устраняет конфликт красного родителя.
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

        auto inserted = std::make_unique<node_type>(std::move(value), rb_color::red);
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

        fix_after_insert(inserted_node);
        return true;
    }

    bool erase(const T& value) override
    {
        node_type* target = this->find(value);
        if (!target)
        {
            return false;
        }

        // replacement может быть nullptr, поэтому отдельно сохраняем его родителя и сторону.
        node_type* replacement = nullptr;
        node_type* replacement_parent = nullptr;
        bool replacement_is_left = false;
        rb_color removed_color;

        node_type* target_parent = target->parent();
        const bool target_was_left = target_parent && target_parent->left() == target;

        if (!target->left() || !target->right())
        {
            removed_color = color_of(target);
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

            replacement = child.get();
            replacement_parent = target_parent;
            replacement_is_left = target_was_left;
            this->attach_subtree(target_parent, target_was_left, std::move(child));
        }
        else
        {
            // Пересаживаем successor вместо target, сохраняя его исходный цвет для fix-up.
            node_type* successor = target->right();
            while (successor->left())
            {
                successor = successor->left();
            }

            removed_color = color_of(successor);
            auto removed_target = this->detach_node(target);
            auto left_subtree = removed_target->detach_left();
            auto right_subtree = removed_target->detach_right();

            auto successor_owner = std::unique_ptr<node_type>{};
            if (successor == right_subtree.get())
            {
                successor_owner = std::move(right_subtree);
                replacement = successor_owner->right();
                replacement_parent = successor_owner.get();
                replacement_is_left = false;
            }
            else
            {
                node_type* successor_parent = successor->parent();
                successor_owner = this->detach_node(successor);

                auto successor_child = successor_owner->detach_right();
                replacement = successor_child.get();
                replacement_parent = successor_parent;
                replacement_is_left = true;
                successor_parent->set_left(std::move(successor_child));
                successor_owner->set_right(std::move(right_subtree));
            }

            successor_owner->set_left(std::move(left_subtree));
            successor_owner->metadata() = color_of(removed_target.get());
            this->attach_subtree(target_parent, target_was_left, std::move(successor_owner));
        }

        if (removed_color == rb_color::black)
        {
            fix_after_erase(replacement, replacement_parent, replacement_is_left);
        }

        return true;
    }

    std::pair<rb_tree, rb_tree> split(const T& pivot)
        requires std::is_copy_constructible_v<Compare>
    {
        rb_tree left_tree(this->comparator());
        rb_tree right_tree(this->comparator());

        struct split_step
        {
            bool belongs_left;
            std::unique_ptr<node_type> pivot;
            std::unique_ptr<node_type> off_path;
        };

        std::vector<split_step> path;
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
                left_root = left_tree.join_with_root(
                    std::move(step->off_path),
                    std::move(step->pivot),
                    std::move(left_root));
            }
            else
            {
                right_root = right_tree.join_with_root(
                    std::move(right_root),
                    std::move(step->pivot),
                    std::move(step->off_path));
            }
        }

        left_tree.root_ = std::move(left_root);
        right_tree.root_ = std::move(right_root);
        return { std::move(left_tree), std::move(right_tree) };
    }

    rb_tree& merge(rb_tree&& right)
    {
        if (this == &right)
        {
            throw std::invalid_argument("A red-black tree cannot be merged with itself");
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
                "Red-black merge requires every left key to precede every right key");
        }

        auto [left_without_maximum, pivot] = detach_max();
        this->root_ = join_with_root(
            std::move(left_without_maximum),
            std::move(pivot),
            std::move(right.root_));
        return *this;
    }

    rb_tree& operator+=(rb_tree&& right)
    {
        return merge(std::move(right));
    }

    std::string to_string() const override
    {
        return tree_serialization::binary_tree_to_string<T, rb_color>(
            "rb_tree",
            this->root());
    }

    std::vector<std::uint8_t> to_binary() const override
    {
        return tree_serialization::binary_tree_to_binary<T, rb_color>(
            tree_serialization::tree_kind::red_black,
            this->root());
    }

private:
    static rb_color color_of(const node_type* node) noexcept
    {
        return node ? node->metadata() : rb_color::black;
    }

    static void set_color(node_type* node, rb_color color) noexcept
    {
        if (node)
        {
            node->metadata() = color;
        }
    }

    static void make_black_root(std::unique_ptr<node_type>& root) noexcept
    {
        if (root)
        {
            root->set_parent(nullptr);
            set_color(root.get(), rb_color::black);
        }
    }

    static std::size_t black_height(const node_type* node) noexcept
    {
        std::size_t result = 0;
        while (node)
        {
            if (color_of(node) == rb_color::black)
            {
                ++result;
            }
            node = node->left();
        }

        return result;
    }

    static void ensure_compatible_comparators(const Compare& left, const Compare& right)
    {
        if constexpr (requires { static_cast<bool>(left == right); })
        {
            if (!(left == right))
            {
                throw std::invalid_argument("Red-black merge requires equivalent comparators");
            }
        }
    }

    std::unique_ptr<node_type> join_with_root(
        std::unique_ptr<node_type> left,
        std::unique_ptr<node_type> pivot,
        std::unique_ptr<node_type> right)
    {
        make_black_root(left);
        make_black_root(right);
        pivot->set_parent(nullptr);
        set_color(pivot.get(), rb_color::red);

        const std::size_t left_black_height = black_height(left.get());
        const std::size_t right_black_height = black_height(right.get());
        if (left_black_height == right_black_height)
        {
            pivot->set_left(std::move(left));
            pivot->set_right(std::move(right));
            set_color(pivot.get(), rb_color::black);
            return pivot;
        }

        if (left_black_height > right_black_height)
        {
            this->root_ = std::move(left);
            node_type* parent = nullptr;
            node_type* current = this->root_.get();
            std::size_t current_black_height = left_black_height;
            while (current
                && (color_of(current) != rb_color::black
                    || current_black_height != right_black_height))
            {
                parent = current;
                if (color_of(current) == rb_color::black)
                {
                    --current_black_height;
                }
                current = current->right();
            }

            if (!parent || current_black_height != right_black_height)
            {
                throw std::logic_error("Red-black join could not find a matching black height");
            }

            auto joined = std::move(pivot);
            joined->set_left(parent->detach_right());
            joined->set_right(std::move(right));
            parent->set_right(std::move(joined));
            fix_after_insert(parent->right());
            return std::move(this->root_);
        }

        this->root_ = std::move(right);
        node_type* parent = nullptr;
        node_type* current = this->root_.get();
        std::size_t current_black_height = right_black_height;
        while (current
            && (color_of(current) != rb_color::black
                || current_black_height != left_black_height))
        {
            parent = current;
            if (color_of(current) == rb_color::black)
            {
                --current_black_height;
            }
            current = current->left();
        }

        if (!parent || current_black_height != left_black_height)
        {
            throw std::logic_error("Red-black join could not find a matching black height");
        }

        auto joined = std::move(pivot);
        joined->set_left(std::move(left));
        joined->set_right(parent->detach_left());
        parent->set_left(std::move(joined));
        fix_after_insert(parent->left());
        return std::move(this->root_);
    }

    std::pair<std::unique_ptr<node_type>, std::unique_ptr<node_type>> detach_max()
    {
        node_type* maximum = this->root_.get();
        while (maximum->right())
        {
            maximum = maximum->right();
        }

        node_type* parent = maximum->parent();
        const bool was_left_child = parent && parent->left() == maximum;
        const rb_color removed_color = color_of(maximum);
        auto pivot = this->detach_node(maximum);
        auto replacement = pivot->detach_left();
        node_type* replacement_node = replacement.get();
        this->attach_subtree(parent, was_left_child, std::move(replacement));

        if (removed_color == rb_color::black)
        {
            fix_after_erase(replacement_node, parent, was_left_child);
        }

        set_color(this->root_.get(), rb_color::black);
        return { std::move(this->root_), std::move(pivot) };
    }

    void fix_after_insert(node_type* node)
    {
        // Пока родитель красный, исправляем пару с помощью перекраски или поворотов.
        while (node != this->root_.get() && color_of(node->parent()) == rb_color::red)
        {
            node_type* parent = node->parent();
            node_type* grandparent = parent->parent();

            if (parent == grandparent->left())
            {
                node_type* uncle = grandparent->right();
                if (color_of(uncle) == rb_color::red)
                {
                    // Красный дядя: перекрашиваем семейство и проверяем выше.
                    set_color(parent, rb_color::black);
                    set_color(uncle, rb_color::black);
                    set_color(grandparent, rb_color::red);
                    node = grandparent;
                }
                else
                {
                    // Чёрный дядя: приводим к прямой линии и поворачиваем дедушку.
                    if (node == parent->right())
                    {
                        node = parent;
                        this->rotate_left(node);
                        parent = node->parent();
                        grandparent = parent->parent();
                    }

                    set_color(parent, rb_color::black);
                    set_color(grandparent, rb_color::red);
                    this->rotate_right(grandparent);
                }
            }
            else
            {
                node_type* uncle = grandparent->left();
                if (color_of(uncle) == rb_color::red)
                {
                    set_color(parent, rb_color::black);
                    set_color(uncle, rb_color::black);
                    set_color(grandparent, rb_color::red);
                    node = grandparent;
                }
                else
                {
                    if (node == parent->left())
                    {
                        node = parent;
                        this->rotate_right(node);
                        parent = node->parent();
                        grandparent = parent->parent();
                    }

                    set_color(parent, rb_color::black);
                    set_color(grandparent, rb_color::red);
                    this->rotate_left(grandparent);
                }
            }
        }

        set_color(this->root_.get(), rb_color::black);
    }

    void fix_after_erase(node_type* node, node_type* parent, bool node_is_left)
    {
        // Удаление чёрного узла создаёт «двойную чёрноту», которую поднимаем или снимаем.
        while (node != this->root_.get() && color_of(node) == rb_color::black)
        {
            if (!parent)
            {
                break;
            }

            const bool is_left = node ? parent->left() == node : node_is_left;
            if (is_left)
            {
                node_type* sibling = parent->right();
                if (color_of(sibling) == rb_color::red)
                {
                    // Красный брат: поворот меняет его на чёрного брата.
                    set_color(sibling, rb_color::black);
                    set_color(parent, rb_color::red);
                    this->rotate_left(parent);
                    sibling = parent->right();
                }

                if (color_of(sibling ? sibling->left() : nullptr) == rb_color::black
                    && color_of(sibling ? sibling->right() : nullptr) == rb_color::black)
                {
                    // Чёрный брат с чёрными детьми: передаём дефицит родителю.
                    set_color(sibling, rb_color::red);
                    node = parent;
                    parent = node->parent();
                    node_is_left = parent && parent->left() == node;
                }
                else
                {
                    // При необходимости ближний племянник становится дальним; поворот снимает дефицит.
                    if (color_of(sibling ? sibling->right() : nullptr) == rb_color::black)
                    {
                        set_color(sibling ? sibling->left() : nullptr, rb_color::black);
                        set_color(sibling, rb_color::red);
                        this->rotate_right(sibling);
                        sibling = parent->right();
                    }

                    set_color(sibling, color_of(parent));
                    set_color(parent, rb_color::black);
                    set_color(sibling ? sibling->right() : nullptr, rb_color::black);
                    this->rotate_left(parent);
                    node = this->root_.get();
                    parent = nullptr;
                }
            }
            else
            {
                node_type* sibling = parent->left();
                if (color_of(sibling) == rb_color::red)
                {
                    // Симметричный случай для правого ребёнка.
                    set_color(sibling, rb_color::black);
                    set_color(parent, rb_color::red);
                    this->rotate_right(parent);
                    sibling = parent->left();
                }

                if (color_of(sibling ? sibling->right() : nullptr) == rb_color::black
                    && color_of(sibling ? sibling->left() : nullptr) == rb_color::black)
                {
                    // Симметричный случай: передаём дефицит родителю.
                    set_color(sibling, rb_color::red);
                    node = parent;
                    parent = node->parent();
                    node_is_left = parent && parent->left() == node;
                }
                else
                {
                    // Зеркально преобразуем ближнего племянника и поворачиваем родителя.
                    if (color_of(sibling ? sibling->left() : nullptr) == rb_color::black)
                    {
                        set_color(sibling ? sibling->right() : nullptr, rb_color::black);
                        set_color(sibling, rb_color::red);
                        this->rotate_left(sibling);
                        sibling = parent->left();
                    }

                    set_color(sibling, color_of(parent));
                    set_color(parent, rb_color::black);
                    set_color(sibling ? sibling->left() : nullptr, rb_color::black);
                    this->rotate_right(parent);
                    node = this->root_.get();
                    parent = nullptr;
                }
            }
        }

        set_color(node, rb_color::black);
    }
};

template <class T, class Compare = std::less<T>>
using red_black_tree = rb_tree<T, Compare>;
