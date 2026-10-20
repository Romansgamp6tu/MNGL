#pragma once

#include <mods.hpp>

#include <cstddef>

namespace mngl_mod_demo
{
    inline constexpr char tree_class_id[] = "demo.tree";
    inline constexpr char insert_extension_id[] = "demo.tree.insert";
    inline constexpr char tree_label_property_id[] = "demo.tree.label";
    inline constexpr char bst_implementation_id[] = "demo.tree.bst";
    inline constexpr char counting_implementation_id[] = "demo.tree.counting";

    using key_type = int;

    class tree_base : public mngl::mods::modable
    {
    public:
        virtual bool insert_impl(key_type key) = 0;
        [[nodiscard]] virtual std::size_t size() const noexcept = 0;
        ~tree_base() override = default;
    };

    using insert_signature = bool(tree_base&, key_type);
}
