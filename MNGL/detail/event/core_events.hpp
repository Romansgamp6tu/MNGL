#pragma once

#include "enums.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace mngl
{
	struct event_base
	{
		event_type type;
		std::uint64_t timestamp;
	};

	struct window_event : event_base
	{
		std::uint32_t window_id;
	};

	struct display_event : event_base
	{
		std::uint32_t display_id;
	};

	// =========================================================================
	// ХЕЛПЕРЫ ВВОДА
	// =========================================================================
	struct keycode
	{
		std::uint32_t value;

		constexpr bool is_scancode() const noexcept { return value & SDLK_SCANCODE_MASK; }

		constexpr bool isScancode() const noexcept { return is_scancode(); }

		constexpr scancode get_scancode() const noexcept
		{
			return static_cast<scancode>(value & ~SDLK_SCANCODE_MASK);
		}

		constexpr char ascii() const noexcept { return static_cast<char>(value); }
	};

	// =========================================================================
	// СТРУКТУРЫ СОБЫТИЙ
	// =========================================================================

	// --- Application ---
	struct app_quit : event_base
	{
	};
	struct app_terminating : event_base
	{
	};
	struct app_low_memory : event_base
	{
	};
	struct app_will_enter_background : event_base
	{
	};
	struct app_did_enter_background : event_base
	{
	};
	struct app_will_enter_foreground : event_base
	{
	};
	struct app_did_enter_foreground : event_base
	{
	};
	struct app_locale_changed : event_base
	{
	};
	struct app_system_theme_changed : event_base
	{
	};

	// --- Display ---
	struct display_orientation_event : display_event
	{
		display_orientation orientation;
	};
	struct display_added : display_event
	{
	};
	struct display_removed : display_event
	{
	};
	struct display_moved : display_event
	{
	};
	struct display_desktop_mode_changed : display_event
	{
	};
	struct display_current_mode_changed : display_event
	{
	};
	struct display_content_scale_changed : display_event
	{
		float content_scale;
	};
	struct display_usable_bounds_changed : display_event
	{
	};

	// --- Window ---
	struct window_shown : window_event
	{
	};
	struct window_hidden : window_event
	{
	};
	struct window_exposed : window_event
	{
	};
	struct window_moved : window_event
	{
		glm::ivec2 position;
	};
	struct window_resized : window_event
	{
		glm::ivec2 size;
	};
	struct window_pixel_size_changed : window_event
	{
		glm::ivec2 size;
	};
	struct window_metal_view_resized : window_event
	{
	};
	struct window_minimized : window_event
	{
	};
	struct window_maximized : window_event
	{
	};
	struct window_restored : window_event
	{
	};
	struct window_mouse_enter : window_event
	{
	};
	struct window_mouse_leave : window_event
	{
	};
	struct window_focus_gained : window_event
	{
	};
	struct window_focus_lost : window_event
	{
	};
	struct window_close_request : window_event
	{
	};
	struct window_hit_test : window_event
	{
	};
	struct window_icc_prof_changed : window_event
	{
	};
	struct window_display_changed : window_event
	{
		std::uint32_t display_id;
	};
	struct window_display_scale_changed : window_event
	{
	};
	struct window_safe_area_changed : window_event
	{
	};
	struct window_occluded : window_event
	{
	};
	struct window_enter_fullscreen : window_event
	{
	};
	struct window_leave_fullscreen : window_event
	{
	};
	struct window_destroyed : window_event
	{
	};
	struct window_hdr_state_changed : window_event
	{
	};

	// --- Keyboard ---

} // namespace mngl

