#include <mod_loader.hpp>

#include <utility>

namespace
{
	using unload_callback = void(MNGL_MOD_CALL*)(void*);
	void* unload_context{};
	unload_callback host_unload_callback{};
} // namespace

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_test_set_unload_callback(void* context,
                                                                            unload_callback callback)
{
	unload_context = context;
	host_unload_callback = callback;
}

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL mngl_mod_load_v1(mngl::mods::mod_context&)
{
	return true;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
	const auto callback = std::exchange(host_unload_callback, nullptr);
	void* const context = std::exchange(unload_context, nullptr);
	if (callback)
		callback(context);
}
