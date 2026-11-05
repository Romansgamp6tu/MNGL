#include <mod_loader.hpp>

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL mngl_mod_load_v1(mngl::mods::mod_context&)
{
	return true;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
}
