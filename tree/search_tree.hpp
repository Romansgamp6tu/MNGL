#pragma once

#include "search_index.hpp"
#include "vertex.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <queue>
#include <stack>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace tt
{
    // Буквы задают порядок посещения: C — узел, L/R — левое/правое поддерево.
    enum class traversal_order
    {
        CLR,
        LCR,
        LRC,
        CRL,
        RCL,
        RLC,
        LEVEL,
        LEVEL_REVERSED,
        preorder = CLR,
        inorder = LCR,
        postorder = LRC,
        level_order = LEVEL,
        reverse_level_order = LEVEL_REVERSED
    };

    inline constexpr traversal_order CLR = traversal_order::CLR;
    inline constexpr traversal_order LCR = traversal_order::LCR;
    inline constexpr traversal_order LRC = traversal_order::LRC;
    inline constexpr traversal_order CRL = traversal_order::CRL;
    inline constexpr traversal_order RCL = traversal_order::RCL;
    inline constexpr traversal_order RLC = traversal_order::RLC;
    inline constexpr traversal_order LEVEL = traversal_order::LEVEL;
    inline constexpr traversal_order LEVEL_REVERSED = traversal_order::LEVEL_REVERSED;
}

template <class T, class Meta = void>
class search_tree : public search_index<T>
{
public:
    using value_type = T;
    using metadata_type = Meta;
    using node_type = vertex<T, Meta>;
    using visitor_type = typename search_index<T>::visitor_type;

    template <bool IsConst>
    class basic_traversal_range
    {
        using node_pointer = std::conditional_t<IsConst, const node_type*, node_type*>;
        using node_list = std::vector<node_pointer>;

    public:
        class iterator
        {
            friend class basic_traversal_range;

        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using reference = std::conditional_t<IsConst, const T&, T&>;
            using pointer = std::conditional_t<IsConst, const T*, T*>;

            iterator() = default;

            reference operator*() const
            {
                // Итератор читает заранее сохранённый узел по текущему индексу.
                return (*nodes_)[index_]->value();
            }

            pointer operator->() const
            {
                return &(*nodes_)[index_]->value();
            }

            iterator& operator++()
            {
                ++index_;
                return *this;
            }

            iterator operator++(int)
            {
                iterator previous = *this;
                ++(*this);
                return previous;
            }

            friend bool operator==(const iterator&, const iterator&) = default;

        private:
            iterator(const node_list* nodes, std::size_t index)
                : nodes_(nodes)
                , index_(index)
            {
            }

            const node_list* nodes_ = nullptr;
            std::size_t index_ = 0;
        };

        iterator begin() const noexcept
        {
            return iterator(&nodes_, 0);
        }

        iterator end() const noexcept
        {
            return iterator(&nodes_, nodes_.size());
        }

        bool empty() const noexcept
        {
            return nodes_.empty();
        }

        std::size_t size() const noexcept
        {
            return nodes_.size();
        }

    private:
        friend class search_tree;

        explicit basic_traversal_range(node_list nodes)
            : nodes_(std::move(nodes))
        {
        }

        node_list nodes_;
    };

    using traversal_range = basic_traversal_range<false>;
    using const_traversal_range = basic_traversal_range<true>;

    ~search_tree() override = default;

    virtual bool insert(T value) override = 0;
    virtual bool erase(const T& value) override = 0;

    const T* lookup(const T& value) override
    {
        return std::as_const(*this).lookup(value);
    }

    const T* lookup(const T& value) const override
    {
        const node_type* current = root_node();

        while (current)
        {
            if (less(value, current->value()))
            {
                current = current->left();
            }
            else if (less(current->value(), value))
            {
                current = current->right();
            }
            else
            {
                return &current->value();
            }
        }

        return nullptr;
    }

    const T* lower_bound(const T& value) const override
    {
        const node_type* candidate = lower_bound_node(value);
        return candidate ? &candidate->value() : nullptr;
    }

    void for_each(const visitor_type& visitor) const override
    {
        std::stack<const node_type*> pending;
        const node_type* current = root_node();

        while (current || !pending.empty())
        {
            while (current)
            {
                pending.push(current);
                current = current->left();
            }

            current = pending.top();
            pending.pop();
            visitor(current->value());
            current = current->right();
        }
    }

    void for_each_range(
        const T& lower,
        const T& upper,
        const visitor_type& visitor) const override
    {
        const node_type* current = lower_bound_node(lower);

        while (current && less(current->value(), upper))
        {
            visitor(current->value());
            current = successor(current);
        }
    }

    // Каждый обход хранит снимок адресов узлов; удаление узлов инвалидирует снимок.
    traversal_range traverse(tt::traversal_order order)
    {
        return traversal_range(collect(root_node(), order));
    }

    const_traversal_range traverse(tt::traversal_order order) const
    {
        return const_traversal_range(collect(root_node(), order));
    }

    friend traversal_range operator|(search_tree& tree, tt::traversal_order order)
    {
        return tree.traverse(order);
    }

    friend const_traversal_range operator|(const search_tree& tree, tt::traversal_order order)
    {
        return tree.traverse(order);
    }

protected:
    virtual node_type* root_node() noexcept = 0;
    virtual const node_type* root_node() const noexcept = 0;
    virtual bool less(const T& left, const T& right) const = 0;

private:
    const node_type* lower_bound_node(const T& value) const
    {
        const node_type* current = root_node();
        const node_type* candidate = nullptr;

        while (current)
        {
            if (!less(current->value(), value))
            {
                candidate = current;
                current = current->left();
            }
            else
            {
                current = current->right();
            }
        }

        return candidate;
    }

    static const node_type* successor(const node_type* node) noexcept
    {
        if (node->right())
        {
            node = node->right();
            while (node->left())
            {
                node = node->left();
            }

            return node;
        }

        const node_type* parent = node->parent();
        while (parent && parent->right() == node)
        {
            node = parent;
            parent = parent->parent();
        }

        return parent;
    }

    template <class Node>
    static std::vector<Node*> collect(Node* root, tt::traversal_order order)
    {
        // Обход выполняется один раз при создании диапазона, без рекурсивных вызовов.
        std::vector<Node*> result;

        switch (order)
        {
        case tt::traversal_order::CLR:
        case tt::traversal_order::CRL:
        case tt::traversal_order::LCR:
        case tt::traversal_order::RCL:
        case tt::traversal_order::LRC:
        case tt::traversal_order::RLC:
        case tt::traversal_order::LEVEL:
        case tt::traversal_order::LEVEL_REVERSED:
            break;
        default:
            throw std::invalid_argument("Unknown tree traversal order");
        }

        if (!root)
        {
            return result;
        }

        if (order == tt::LEVEL || order == tt::LEVEL_REVERSED)
        {
            // BFS использует очередь; обратный порядок — развёрнутый результат BFS.
            std::queue<Node*> pending;
            pending.push(root);

            while (!pending.empty())
            {
                Node* current = pending.front();
                pending.pop();
                result.push_back(current);

                if (current->left())
                {
                    pending.push(current->left());
                }

                if (current->right())
                {
                    pending.push(current->right());
                }
            }

            if (order == tt::LEVEL_REVERSED)
            {
                std::reverse(result.begin(), result.end());
            }

            return result;
        }

        if (order == tt::CLR || order == tt::CRL)
        {
            // Для preorder кладём детей в стек в обратном порядке посещения.
            std::stack<Node*> pending;
            pending.push(root);

            while (!pending.empty())
            {
                Node* current = pending.top();
                pending.pop();
                result.push_back(current);

                if (order == tt::CLR && current->right())
                {
                    pending.push(current->right());
                }

                if (order == tt::CLR && current->left())
                {
                    pending.push(current->left());
                }
                else if (order == tt::CRL)
                {
                    if (current->left())
                    {
                        pending.push(current->left());
                    }
                    if (current->right())
                    {
                        pending.push(current->right());
                    }
                }
            }

            return result;
        }

        if (order == tt::LCR || order == tt::RCL)
        {
            // Итеративный inorder: стек хранит путь до следующего узла.
            std::stack<Node*> pending;
            Node* current = root;

            while (current || !pending.empty())
            {
                while (current)
                {
                    pending.push(current);
                    current = order == tt::LCR ? current->left() : current->right();
                }

                current = pending.top();
                pending.pop();
                result.push_back(current);
                current = order == tt::LCR ? current->right() : current->left();
            }

            return result;
        }

        // Postorder строится как обратный preorder с противоположным порядком детей.
        std::stack<Node*> pending;
        std::stack<Node*> reversed;
        pending.push(root);

        while (!pending.empty())
        {
            Node* current = pending.top();
            pending.pop();
            reversed.push(current);

            if (order == tt::LRC && current->left())
            {
                pending.push(current->left());
            }

            if (order == tt::LRC && current->right())
            {
                pending.push(current->right());
            }

            if (order == tt::RLC && current->right())
            {
                pending.push(current->right());
            }

            if (order == tt::RLC && current->left())
            {
                pending.push(current->left());
            }
        }

        while (!reversed.empty())
        {
            result.push_back(reversed.top());
            reversed.pop();
        }

        return result;
    }
};
