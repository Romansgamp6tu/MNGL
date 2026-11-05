#include "tree_api.hpp"

#include <mod_loader.hpp>

#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <string>

namespace
{
	class counting_tree final : public mngl_mod_demo::tree_base
	{
	public:
		bool insert_impl(mngl_mod_demo::key_type key) override { return m_keys.insert(key).second; }

		[[nodiscard]] std::size_t size() const noexcept override { return m_keys.size(); }

	private:
		std::set<mngl_mod_demo::key_type> m_keys;
	};
} // namespace

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	if (!context.replace_function<mngl_mod_demo::insert_signature>(
	        mngl_mod_demo::insert_extension_id,
	        [](mngl_mod_demo::tree_base& tree, mngl_mod_demo::key_type key)
	        {
		        const auto normalized =
		            key == std::numeric_limits<mngl_mod_demo::key_type>::min() ? key : (key < 0 ? -key : key);
		        std::clog << "[example.tree-tools] replacement insert " << key << " as " << normalized
		                  << '\n';
		        return tree.insert_impl(normalized);
	        }))
		return false;

	context.around_function<mngl_mod_demo::insert_signature>(
	    mngl_mod_demo::insert_extension_id, 10,
	    [](auto next, mngl_mod_demo::tree_base& tree, mngl_mod_demo::key_type key)
	    {
		    std::clog << "[example.tree-tools] around insert " << key << '\n';
		    return next(tree, key);
	    });

	if (!context.set_property<std::string>(mngl_mod_demo::tree_label_property_id, "Tree tools mod active"))
		return false;
	if (!context.set_property<float>(mngl_mod_demo::tree_node_gap_property_id, 32.0f))
		return false;

	return context.add_implementation<mngl_mod_demo::tree_base>(
	    mngl_mod_demo::tree_class_id, mngl_mod_demo::counting_implementation_id,
	    [] { return std::make_unique<counting_tree>(); });
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
	std::clog << "[example.tree-tools] unloaded\n";
}
