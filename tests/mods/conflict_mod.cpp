#include "tree_api.hpp"

#include <mod_loader.hpp>

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL
mngl_mod_load_v1(mngl::mods::mod_context& context)
{
    // Deliberately conflict with example.tree-tools. The host must diagnose
    // the conflict and disable both replacement callbacks for this slot.
    (void)context.replace_function<mngl_mod_demo::insert_signature>(
        mngl_mod_demo::insert_extension_id,
        [](mngl_mod_demo::tree_base& tree, mngl_mod_demo::key_type key)
        {
            return tree.insert_impl(key + 1000);
        });
    return true;
}
