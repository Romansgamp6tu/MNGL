#pragma once

#include "core_events.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace mngl
{
	struct clipboard_update : event_base
	{
		bool owner;
		std::vector<std::string> mime_types;
	};

	// --- Drag & Drop ---
	struct drop_file : window_event
	{
		glm::vec2 position;
		std::string source;
		std::string data;
	};
	struct drop_text : window_event
	{
		glm::vec2 position;
		std::string source;
		std::string data;
	};
	struct drop_begin : window_event
	{
		glm::vec2 position;
		std::string source;
	};
	struct drop_complete : window_event
	{
		glm::vec2 position;
		std::string source;
	};
	struct drop_position : window_event
	{
		glm::vec2 position;
		std::string source;
		std::string data;
	};

	// --- Audio ---
#if !defined(MNGL_CUSTOM_AUDIO_BACKEND)
	struct audio_device_added : event_base
	{
		std::uint32_t device_id;
		bool recording;
	};
	struct audio_device_removed : event_base
	{
		std::uint32_t device_id;
		bool recording;
	};
	struct audio_device_format_changed : event_base
	{
		std::uint32_t device_id;
		bool recording;
	};
#endif

	// --- Sensor ---
	struct sensor_event : event_base
	{
		std::uint32_t sensor_id;
		float data[6];
		std::uint64_t timestamp;
	};

	// --- Pen ---
	// SDL proximity events have no coordinates; position is zero for compatibility.
	struct pen_proximity_in : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
	};
	struct pen_proximity_out : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
	};
	struct pen_motion : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
		SDL_PenInputFlags state;
	};
	struct pen_down : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
		SDL_PenInputFlags state;
		bool eraser;
	};
	struct pen_up : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
		SDL_PenInputFlags state;
		bool eraser;
	};
	struct pen_button_down : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
		std::uint8_t button;
		SDL_PenInputFlags state;
	};
	struct pen_button_up : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
		std::uint8_t button;
		SDL_PenInputFlags state;
	};
	struct pen_axis : event_base
	{
		SDL_PenID pen_id;
		SDL_WindowID window_id;
		glm::vec2 position;
		pen_axis_type axis;
		float value;
		SDL_PenInputFlags state;
	};

	// --- Camera ---
	struct camera_device_added : event_base
	{
		std::uint32_t camera_id;
	};
	struct camera_device_removed : event_base
	{
		std::uint32_t camera_id;
	};
	struct camera_device_approved : event_base
	{
		std::uint32_t camera_id;
	};
	struct camera_device_denied : event_base
	{
		std::uint32_t camera_id;
	};

	// --- Render ---
	struct render_targets_reset : window_event
	{
	};
	struct render_device_reset : window_event
	{
	};
	struct render_device_lost : window_event
	{
	};

	// --- Internal ---
	struct poll_sentinel : event_base
	{
	};
	struct user_event : event_base
	{
		std::uint32_t window_id;
		std::int32_t code;
		void* data1;
		void* data2;
	};

	// =========================================================================
	// 1. КОНЦЕПТ ДЛЯ ПРОВЕРКИ ТИПА СОБЫТИЯ
	// =========================================================================

} // namespace mngl
