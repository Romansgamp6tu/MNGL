// Compile the whole owned public surface, including audio, without opening a GPU or linking a mixer.
#include <MNGL/action_map.hpp>
#include <MNGL/async_io.hpp>
#include <MNGL/audio.hpp>
#include <MNGL/clipboard.hpp>
#include <MNGL/compute_pass.hpp>
#include <MNGL/console.hpp>
#include <MNGL/dialog.hpp>
#include <MNGL/extras/tree_renderer.hpp>
#include <MNGL/frame_graph.hpp>
#include <MNGL/imgui_compat.hpp>
#include <MNGL/line.hpp>
#include <MNGL/mod_loader.hpp>
#include <MNGL/paths.hpp>
#include <MNGL/process.hpp>
#include <MNGL/render_window.hpp>
#include <MNGL/storage.hpp>

#include <concepts>
#include <type_traits>

static_assert(std::same_as<mngl::Input, mngl::input>);
static_assert(std::same_as<mngl::windowResized, mngl::window_resized>);
static_assert(std::same_as<mngl::displayOrientation, mngl::display_orientation_event>);
static_assert(std::same_as<mngl::gamepadAxis, mngl::gamepad_axis_event>);
static_assert(std::same_as<mngl::ActionMap, mngl::action_map>);
static_assert(std::same_as<mngl::two_d::Frame2D, mngl::two_d::frame_2d>);
static_assert(std::same_as<mngl::process::Process, mngl::process::child_process>);
static_assert(std::same_as<mngl::storage::Storage, mngl::storage::store>);
static_assert(std::same_as<decltype(mngl::paths::user_data_directory()), std::filesystem::path>);
static_assert(std::same_as<decltype(mngl::paths::try_user_data_directory()),
                           std::expected<std::filesystem::path, mngl::sdl_error>>);
static_assert(
    std::same_as<decltype(std::declval<const mngl::dialog::request&>().result()), mngl::dialog::result>);
static_assert(!std::copy_constructible<mngl::process::child_process>);
static_assert(std::is_nothrow_move_constructible_v<mngl::process::child_process>);
static_assert(std::copy_constructible<mngl::gpu::device>);
static_assert(
    std::same_as<decltype(std::declval<const mngl::imgui_compat::widgets::button&>().is_small()), bool>);
static_assert(std::same_as<decltype(std::declval<const mngl::gpu::graphics_pipeline&>().raw()),
                           SDL_GPUGraphicsPipeline*>);
static_assert(std::same_as<mngl::async_io::request_outcome,
                           std::expected<mngl::async_io::request_value, mngl::async_io::request_error>>);

#if !defined(MNGL_CUSTOM_AUDIO_BACKEND) && !defined(MNGL_CUSTOM_AUDIO_INTERFACE) &&                          \
    !defined(MNGL_CUSTOM_MIXER_INTERFACE)
static_assert(std::same_as<decltype(mngl::audio::load_sound("test.wav")), mngl::audio::sound>);
static_assert(std::same_as<decltype(mngl::audio::try_load_sound("test.wav")),
                           std::expected<mngl::audio::sound, mngl::audio::error>>);
static_assert(std::same_as<decltype(mngl::audio::pause({})), void>);
static_assert(std::same_as<decltype(mngl::audio::try_pause({})), std::expected<void, mngl::audio::error>>);
static_assert(std::same_as<mngl::audio::Sound, mngl::audio::sound>);
#endif
