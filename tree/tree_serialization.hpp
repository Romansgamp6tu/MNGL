#pragma once

#include "vertex.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace tree_serialization
{
    enum class tree_kind : std::uint8_t
    {
        bst = 1,
        avl = 2,
        red_black = 3,
        treap = 4,
        splay = 5,
        b_plus = 6
    };

    class binary_writer
    {
    public:
        binary_writer(
            tree_kind kind,
            std::string_view value_type,
            std::size_t value_size,
            std::string_view metadata_type = {},
            std::size_t metadata_size = 0)
        {
            write_u8('T');
            write_u8('R');
            write_u8('E');
            write_u8('E');
            write_u16(1);
            write_u8(static_cast<std::uint8_t>(kind));
            write_u8(1); // Payloads use native object representations; this is same-ABI data.
            write_u32(static_cast<std::uint32_t>(value_size));
            write_u32(static_cast<std::uint32_t>(metadata_size));
            write_string(value_type);
            write_string(metadata_type);
        }

        void write_u8(std::uint8_t value)
        {
            bytes_.push_back(value);
        }

        void write_u16(std::uint16_t value)
        {
            write_u8(static_cast<std::uint8_t>(value));
            write_u8(static_cast<std::uint8_t>(value >> 8));
        }

        void write_u32(std::uint32_t value)
        {
            for (unsigned shift = 0; shift < 32; shift += 8)
            {
                write_u8(static_cast<std::uint8_t>(value >> shift));
            }
        }

        void write_u64(std::uint64_t value)
        {
            for (unsigned shift = 0; shift < 64; shift += 8)
            {
                write_u8(static_cast<std::uint8_t>(value >> shift));
            }
        }

        void write_string(std::string_view value)
        {
            write_u32(static_cast<std::uint32_t>(value.size()));
            bytes_.insert(bytes_.end(), value.begin(), value.end());
        }

        template <class Value>
        void write_object(const Value& value)
        {
            if constexpr (std::is_trivially_copyable_v<Value>)
            {
                const std::size_t offset = bytes_.size();
                bytes_.resize(offset + sizeof(Value));
                std::memcpy(bytes_.data() + offset, std::addressof(value), sizeof(Value));
            }
            else
            {
                throw std::logic_error(
                    "Binary tree export requires trivially copyable values and metadata");
            }
        }

        std::vector<std::uint8_t> take() &&
        {
            return std::move(bytes_);
        }

    private:
        std::vector<std::uint8_t> bytes_;
    };

    template <class Value>
    void write_text_value(std::ostream& output, const Value& value)
    {
        using value_type = std::remove_cvref_t<Value>;

        if constexpr (std::is_enum_v<value_type>)
        {
            output << static_cast<std::underlying_type_t<value_type>>(value);
        }
        else if constexpr (std::is_same_v<value_type, std::string>)
        {
            output << std::quoted(value);
        }
        else if constexpr (std::is_floating_point_v<value_type>)
        {
            const auto previous_precision = output.precision();
            const auto previous_flags = output.flags();
            output << std::setprecision(
                std::numeric_limits<value_type>::max_digits10)
                << std::showpoint
                << value;
            output.precision(previous_precision);
            output.flags(previous_flags);
        }
        else if constexpr (requires { output << value; })
        {
            output << value;
        }
        else
        {
            throw std::logic_error("Text tree export requires stream-insertable values");
        }
    }

    template <class T, class Meta>
    std::string binary_tree_to_string(
        std::string_view tree_name,
        const vertex<T, Meta>* root)
    {
        struct event
        {
            const vertex<T, Meta>* node;
            char punctuation;
        };

        std::ostringstream output;
        output << tree_name << '{';
        std::vector<event> pending;
        pending.push_back({ nullptr, '}' });
        pending.push_back({ root, '\0' });

        while (!pending.empty())
        {
            const event current = pending.back();
            pending.pop_back();

            if (current.punctuation)
            {
                output << current.punctuation;
                continue;
            }

            if (!current.node)
            {
                output << '#';
                continue;
            }

            output << '(';
            write_text_value(output, current.node->value());
            if constexpr (!std::is_void_v<Meta>)
            {
                output << '|';
                write_text_value(output, current.node->metadata());
            }

            output << ',';
            pending.push_back({ nullptr, ')' });
            pending.push_back({ current.node->right(), '\0' });
            pending.push_back({ nullptr, ',' });
            pending.push_back({ current.node->left(), '\0' });
            pending.push_back({ nullptr, ',' });
        }

        return output.str();
    }

    template <class T, class Meta>
    std::vector<std::uint8_t> binary_tree_to_binary(
        tree_kind kind,
        const vertex<T, Meta>* root)
    {
        if constexpr (!std::is_trivially_copyable_v<T>
            || (!std::is_void_v<Meta> && !std::is_trivially_copyable_v<Meta>))
        {
            throw std::logic_error(
                "Binary tree export requires trivially copyable values and metadata");
        }
        else
        {
            const std::string_view metadata_type = [] {
                if constexpr (std::is_void_v<Meta>)
                {
                    return std::string_view{};
                }
                else
                {
                    return std::string_view(typeid(Meta).name());
                }
            }();

            binary_writer output(
                kind,
                typeid(T).name(),
                sizeof(T),
                metadata_type,
                [] {
                    if constexpr (std::is_void_v<Meta>)
                    {
                        return std::size_t{ 0 };
                    }
                    else
                    {
                        return sizeof(Meta);
                    }
                }());

            std::vector<const vertex<T, Meta>*> pending;
            pending.push_back(root);

            while (!pending.empty())
            {
                const vertex<T, Meta>* current = pending.back();
                pending.pop_back();
                output.write_u8(current ? 1 : 0);

                if (!current)
                {
                    continue;
                }

                output.write_object(current->value());
                if constexpr (!std::is_void_v<Meta>)
                {
                    output.write_object(current->metadata());
                }

                pending.push_back(current->right());
                pending.push_back(current->left());
            }

            return std::move(output).take();
        }
    }

    template <class Node>
    void write_b_plus_text_node(
        std::ostream& output,
        const Node* node)
    {
        if (!node)
        {
            output << '#';
            return;
        }

        const auto& keys = node->keys();
        if (node->is_leaf())
        {
            output << "L{";
            for (std::size_t i = 0; i < keys.size(); ++i)
            {
                if (i)
                {
                    output << ',';
                }
                write_text_value(output, keys[i]);
            }
            output << '}';
            return;
        }

        output << "I{";
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            if (i)
            {
                output << ',';
            }
            write_text_value(output, keys[i]);
        }
        output << "}[";
        for (std::size_t i = 0; i < node->child_count(); ++i)
        {
            if (i)
            {
                output << ',';
            }
            write_b_plus_text_node(output, node->child(i));
        }
        output << ']';
    }

    template <class Node>
    void write_b_plus_binary_node(binary_writer& output, const Node* node)
    {
        output.write_u8(node ? 1 : 0);
        if (!node)
        {
            return;
        }

        output.write_u8(node->is_leaf() ? 1 : 0);
        output.write_u64(static_cast<std::uint64_t>(node->keys().size()));
        for (const auto& key : node->keys())
        {
            output.write_object(key);
        }

        output.write_u64(static_cast<std::uint64_t>(node->child_count()));
        for (std::size_t i = 0; i < node->child_count(); ++i)
        {
            write_b_plus_binary_node(output, node->child(i));
        }
    }
}
