#pragma once

#include "context.hpp"

namespace mngl
{
	inline imgui_context::imgui_context(render_window& window, const imgui_compat::font_config& font_config)
	{
		if (!SDL_IsMainThread()) [[unlikely]]
		{
			throw std::logic_error("Dear ImGui must be initialized on the SDL main thread");
		}
		if (ImGui::GetCurrentContext()) [[unlikely]]
		{
			throw std::logic_error("MNGL currently supports one active Dear ImGui context");
		}

		color_target_format = window.gpu_device().swapchain_texture_format(window);
		if (color_target_format == gpu::texture_format::invalid) [[unlikely]]
		{
			throw std::runtime_error("Cannot initialize Dear ImGui for an invalid swapchain format");
		}

		context = ImGui::CreateContext();
		if (!context) [[unlikely]]
		{
			throw std::runtime_error("Failed to create a Dear ImGui context");
		}

		make_current();
		try
		{
			attached_window = &window;
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = "MNGL_imgui.ini";
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			m_applied_scale = effective_scale();
			imgui_compat::detail::apply_vs_dark_style(m_applied_scale);
			initialize_fonts(font_config);

			if (!ImGui_ImplSDL3_InitForSDLGPU(window.raw())) [[unlikely]]
			{
				throw sdl_error("Failed to initialize the Dear ImGui SDL3 backend: ");
			}
			ImGui_ImplSDL3_SetMouseCaptureMode(ImGui_ImplSDL3_MouseCaptureMode_EnabledAfterDrag);

			ImGui_ImplSDLGPU3_InitInfo init_info{};
			init_info.Device = window.gpu_device().raw();
			init_info.ColorTargetFormat = static_cast<SDL_GPUTextureFormat>(color_target_format);
			init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
			if (!ImGui_ImplSDLGPU3_Init(&init_info)) [[unlikely]]
			{
				throw sdl_error("Failed to initialize the Dear ImGui SDL_GPU backend: ");
			}

			event_manager::instance();
			event_manager::set_imgui_input_enabled(true);
			window.attach_overlay(*this);
			attached_window = &window;
		}
		catch (...)
		{
			shutdown();
			throw;
		}
	}

	inline void imgui_context::initialize_fonts(const imgui_compat::font_config& config)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImFontAtlas& atlas = *io.Fonts;
		fonts[0] = {imgui_compat::detail::load_font(atlas, config.regular), config.regular.size_pixels};
		fonts[1] = {imgui_compat::detail::load_font(atlas, config.bold, fonts[0].value),
		            config.bold.size_pixels};
		fonts[2] = {imgui_compat::detail::load_font(atlas, config.italic, fonts[0].value),
		            config.italic.size_pixels};
		fonts[3] = {imgui_compat::detail::load_font(atlas, config.bold_italic, fonts[0].value),
		            config.bold_italic.size_pixels};
		fonts[4] = {imgui_compat::detail::load_font(atlas, config.monospace, fonts[0].value),
		            config.monospace.size_pixels};

		io.FontDefault = fonts[0].value;
		ImGui::GetStyle().FontSizeBase = config.regular.size_pixels;
	}

	inline const imgui_context::loaded_font& imgui_context::get_font(imgui_compat::font_role role) const
	{
		const auto index = static_cast<std::size_t>(role);
		if (index >= fonts.size()) [[unlikely]]
		{
			throw std::invalid_argument("Invalid Dear ImGui font role");
		}
		return fonts[index];
	}

	inline imgui_context::~imgui_context() noexcept
	{
		if (attached_window)
		{
			attached_window->detach_overlay(*this);
			attached_window = nullptr;
		}
		shutdown();
	}

	inline imgui_context::frame_scope imgui_context::frame()
	{
		begin_frame(nullptr);
		return frame_scope(*this);
	}

	inline imgui_context::frame_scope imgui_context::frame(::mngl::texture& target)
	{
		begin_frame(&target);
		return frame_scope(*this);
	}

	inline void imgui_context::begin_frame(::mngl::texture* target)
	{
		if (!context || !attached_window) [[unlikely]]
		{
			throw std::logic_error("Cannot begin a frame without an attached render_window");
		}
		if (frame_active) [[unlikely]]
		{
			throw std::logic_error("A Dear ImGui frame is already active");
		}
		if (!SDL_IsMainThread()) [[unlikely]]
		{
			throw std::logic_error("Dear ImGui frames must run on the SDL main thread");
		}
		if (target)
		{
			if (!attached_window->hit_testing_enabled()) [[unlikely]]
			{
				throw std::logic_error(
				    "Enable render_window hit testing before drawing an ImGui surface sprite");
			}

			const auto& gpu_texture = target->raw_gpu_texture();
			if (!target->valid() || !target->can_render_to() ||
			    !gpu_texture.has_usage(gpu::texture::usage_flags::sampler) || target->rendering() ||
			    gpu_texture.is_swapchain_texture()) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Dear ImGui offscreen output must be an idle, sampleable color-target texture");
			}
			if (gpu_texture.device_raw() != attached_window->gpu_device().raw()) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Dear ImGui offscreen output and render_window must use the same GPU device");
			}
			if (target->format() != color_target_format) [[unlikely]]
			{
				throw std::invalid_argument(
				    "Dear ImGui offscreen output must use the render_window swapchain format");
			}
		}

		make_current();
		if (target)
		{
			if (captured_texture_identity != target->m_hit_test_identity)
			{
				captured_texture_identity = target->m_hit_test_identity;
				captured_hit_test_path.clear();
				offscreen_mouse_captured = false;
				has_last_offscreen_mouse_position = false;
			}
		}
		else
		{
			captured_texture_identity = 0;
			captured_hit_test_path.clear();
			offscreen_mouse_captured = false;
			has_last_offscreen_mouse_position = false;
		}

		ImGui_ImplSDL3_NewFrame();
		ImGui_ImplSDLGPU3_NewFrame();

		const float needed_scale = effective_scale();
		if (std::abs(needed_scale - m_applied_scale) > 0.001f)
		{
			m_applied_scale = needed_scale;
			imgui_compat::detail::apply_vs_dark_style(m_applied_scale);
		}

		if (target)
		{
			const auto target_size = target->size();
			ImGuiIO& io = ImGui::GetIO();
			io.DisplaySize = ImVec2(static_cast<float>(target_size.x), static_cast<float>(target_size.y));
			io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
			const float offscreen_cursor = -std::numeric_limits<float>::max();
			io.AddMousePosEvent(offscreen_cursor, offscreen_cursor);
			bool cursor_routed = false;

			if (attached_window->m_hit_test_frame)
			{
				const auto& hit_frame = *attached_window->m_hit_test_frame;
				const auto logical_size = attached_window->size();
				if (logical_size.x != 0 && logical_size.y != 0)
				{
					const auto mice = input::mice();
					const glm::vec2 mouse_position = mice.empty() ? glm::vec2{} : mice.front().position();
					const glm::vec2 window_pixel_position{
					    mouse_position.x * static_cast<float>(hit_frame.target_size.x) /
					        static_cast<float>(logical_size.x),
					    (static_cast<float>(logical_size.y) - mouse_position.y) *
					        static_cast<float>(hit_frame.target_size.y) / static_cast<float>(logical_size.y)};
					auto add_routed_position = [&](const hit_test_path_entry& entry)
					{
						if (entry.source_size.x == 0 || entry.source_size.y == 0)
						{
							return false;
						}

						const glm::vec2 routed_position =
						    entry.source_pixel_coords * glm::vec2(target_size) / glm::vec2(entry.source_size);
						last_offscreen_mouse_position = {
						    routed_position.x, static_cast<float>(target_size.y) - routed_position.y};
						has_last_offscreen_mouse_position = true;
						io.AddMousePosEvent(last_offscreen_mouse_position.x, last_offscreen_mouse_position.y);
						return true;
					};

					if (offscreen_mouse_captured &&
					    captured_texture_identity == target->m_hit_test_identity &&
					    !captured_hit_test_path.empty())
					{
						if (const auto surface_path = detail::map_hit_test_path_to_source_unbounded(
						        hit_frame, captured_hit_test_path, window_pixel_position,
						        target->m_hit_test_identity))
						{
							cursor_routed = add_routed_position(*surface_path);
						}
					}
					else if (const auto hit = attached_window->hit_test(mouse_position))
					{
						const auto surface_path = std::find_if(
						    hit->path.begin(), hit->path.end(), [target](const hit_test_path_entry& entry)
						    { return entry.source_identity == target->m_hit_test_identity; });
						if (surface_path != hit->path.end() && add_routed_position(*surface_path))
						{
							captured_hit_test_path = hit->path;
							cursor_routed = true;
						}
					}
				}
			}

			if (!cursor_routed && offscreen_mouse_captured && has_last_offscreen_mouse_position)
			{
				io.AddMousePosEvent(last_offscreen_mouse_position.x, last_offscreen_mouse_position.y);
				cursor_routed = true;
			}
			if (!cursor_routed && !offscreen_mouse_captured)
			{
				captured_hit_test_path.clear();
				has_last_offscreen_mouse_position = false;
			}
		}
		ImGui::NewFrame();
		frame_active = true;
		draw_data_ready = false;
		requested_render_target = target;
	}

	inline void imgui_context::finish_frame() noexcept
	{
		if (!context || !frame_active)
		{
			return;
		}

		make_current();
		ImGui::Render();
		frame_active = false;
		draw_data_ready = true;

		const ImGuiIO& io = ImGui::GetIO();
		input::set_gui_capture(io.WantCaptureMouse, io.WantCaptureKeyboard);
		offscreen_mouse_captured = false;
		if (requested_render_target && !captured_hit_test_path.empty())
		{
			for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
			{
				if (io.MouseDown[button])
				{
					offscreen_mouse_captured = true;
					break;
				}
			}
		}
		if (!offscreen_mouse_captured)
		{
			captured_hit_test_path.clear();
			has_last_offscreen_mouse_position = false;
		}
	}

	inline void imgui_context::render(gpu::command_buffer& command_buffer, ::mngl::texture& target)
	{
		if (!context || !draw_data_ready)
		{
			return;
		}
		if (requested_render_target)
		{
			if (requested_render_target != &target) [[unlikely]]
			{
				throw std::logic_error("Dear ImGui frame must be displayed to the texture passed to frame()");
			}
		}
		else if (!target.raw_gpu_texture().is_swapchain_texture())
		{
			return;
		}
		if (!SDL_IsMainThread()) [[unlikely]]
		{
			throw std::logic_error("Dear ImGui rendering must run on the SDL main thread");
		}

		draw_data_ready = false;
		requested_render_target = nullptr;
		make_current();
		ImDrawData* data = ImGui::GetDrawData();
		if (!data || data->TotalVtxCount == 0 || data->DisplaySize.x <= 0.0f || data->DisplaySize.y <= 0.0f)
		{
			return;
		}
		if (target.format() != color_target_format) [[unlikely]]
		{
			throw std::logic_error("Dear ImGui render target format changed; recreate its context");
		}

		SDL_GPUCommandBuffer* raw_command_buffer = command_buffer.raw();
		ImGui_ImplSDLGPU3_PrepareDrawData(data, raw_command_buffer);

		SDL_GPUColorTargetInfo color_target{};
		color_target.texture = target.raw_gpu_texture().raw();
		color_target.load_op = SDL_GPU_LOADOP_LOAD;
		color_target.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* render_pass =
		    SDL_BeginGPURenderPass(raw_command_buffer, &color_target, 1, nullptr);
		if (!render_pass) [[unlikely]]
		{
			throw sdl_error("Failed to begin the Dear ImGui GPU render pass: ");
		}

		ImGui_ImplSDLGPU3_RenderDrawData(data, raw_command_buffer, render_pass);
		SDL_EndGPURenderPass(render_pass);
	}

	inline void imgui_context::detach_from_window() noexcept
	{
		attached_window = nullptr;
		shutdown();
	}

	inline void imgui_context::shutdown() noexcept
	{
		event_manager::set_imgui_input_enabled(false);
		input::set_gui_capture(false, false);
		requested_render_target = nullptr;
		captured_hit_test_path.clear();
		captured_texture_identity = 0;
		offscreen_mouse_captured = false;
		has_last_offscreen_mouse_position = false;
		if (!context)
		{
			return;
		}

		make_current();
		if (frame_active)
		{
			ImGui::EndFrame();
			frame_active = false;
		}

		ImGuiIO& io = ImGui::GetIO();
		if (io.IniFilename)
		{
			ImGui::SaveIniSettingsToDisk(io.IniFilename);
		}
		if (io.BackendRendererUserData)
		{
			ImGui_ImplSDLGPU3_Shutdown();
		}
		if (io.BackendPlatformUserData)
		{
			ImGui_ImplSDL3_SetMouseCaptureMode(ImGui_ImplSDL3_MouseCaptureMode_Disabled);
			ImGui_ImplSDL3_Shutdown();
		}
		ImGui::DestroyContext(context);
		context = nullptr;
		fonts = {};
		draw_data_ready = false;
	}

	inline void imgui_context::make_current() const noexcept
	{
		if (context)
		{
			ImGui::SetCurrentContext(context);
		}
	}

	inline imgui_context::window_scope::window_scope(imgui_context& owner, std::string_view title,
	                                                 const imgui_compat::window_options& options,
	                                                 std::optional<ImGuiID> dockspace_id, bool* open)
	    : owner(&owner), visible(false), end(end_action::window)
	{
		if (!owner.context || !owner.frame_active) [[unlikely]]
		{
			throw std::logic_error("An ImGui window must be created inside an active frame");
		}

		owner.make_current();
		if (dockspace_id)
		{
			ImGui::SetNextWindowDockID(*dockspace_id, ImGuiCond_FirstUseEver);
		}
		const std::string terminated_title = imgui_compat::detail::widget_label(title);
		visible = ImGui::Begin(terminated_title.c_str(), open,
		                       imgui_compat::detail::to_imgui_window_flags(options));
	}

	inline imgui_context::window_scope::window_scope(imgui_context& owner, bool visible,
	                                                 end_action end) noexcept
	    : owner(&owner), visible(visible), end(end)
	{
	}

	inline imgui_context::main_menu_bar_scope::main_menu_bar_scope(imgui_context& owner)
	    : owner(&owner), visible(false)
	{
		if (!owner.context || !owner.frame_active) [[unlikely]]
		{
			throw std::logic_error("An ImGui main menu bar must be created inside an active frame");
		}

		owner.make_current();
		visible = ImGui::BeginMainMenuBar();
	}

	inline imgui_context::main_menu_bar_scope::~main_menu_bar_scope() noexcept
	{
		if (owner && owner->context && visible)
		{
			owner->make_current();
			ImGui::EndMainMenuBar();
		}
	}

	inline imgui_context::menu_scope::menu_scope(imgui_context& owner, std::string_view label)
	    : owner(&owner), visible(false)
	{
		if (!owner.context || !owner.frame_active) [[unlikely]]
		{
			throw std::logic_error("An ImGui menu must be created inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui menu labels cannot contain embedded null characters");
		}

		owner.make_current();
		const std::string terminated_label(label);
		visible = ImGui::BeginMenu(terminated_label.c_str());
	}

	inline imgui_context::menu_scope::~menu_scope() noexcept
	{
		if (owner && owner->context && visible)
		{
			owner->make_current();
			ImGui::EndMenu();
		}
	}

	inline bool imgui_context::menu_scope::toggle(std::string_view label, bool& value) const
	{
		if (!owner || !owner->context || !visible)
		{
			return false;
		}
		if (!owner->frame_active) [[unlikely]]
		{
			throw std::logic_error("An ImGui menu item must be submitted inside an active frame");
		}
		if (label.find('\0') != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument(
			    "Dear ImGui menu item labels cannot contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_label(label);
		return ImGui::MenuItem(terminated_label.c_str(), nullptr, &value);
	}

	inline imgui_context::dockspace_scope imgui_context::window_scope::dockspace(std::string_view id) const
	{
		if (!owner || !owner->context || !owner->frame_active) [[unlikely]]
		{
			throw std::logic_error("An ImGui dockspace must be created inside an active frame");
		}
		if (id.empty() || id.find('\0') != std::string_view::npos) [[unlikely]]
		{
			throw std::invalid_argument(
			    "Dear ImGui dockspace IDs cannot be empty or contain embedded null characters");
		}

		owner->make_current();
		const std::string terminated_id(id);
		const ImGuiID dockspace_id = ImGui::GetID(terminated_id.c_str());
		// Keep the dock node alive even while its host window is collapsed.
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		return dockspace_scope(*owner, dockspace_id);
	}

	inline imgui_context::window_scope imgui_context::dockspace_scope::window(
	    std::string_view title, const imgui_compat::window_options& options) const
	{
		return window_scope(*owner, title, options, id);
	}

	inline imgui_context::window_scope imgui_context::dockspace_scope::window(std::string_view title,
	                                                                          bool& open,
	                                                                          const imgui_compat::window_options& options) const
	{
		return window_scope(*owner, title, options, id, &open);
	}

	inline imgui_context::window_scope::~window_scope() noexcept
	{
		if (owner && owner->context)
		{
			owner->make_current();
			switch (end)
			{
			case end_action::window: ImGui::End(); break;
			case end_action::child: ImGui::EndChild(); break;
			case end_action::combo: ImGui::EndCombo(); break;
			case end_action::popup: ImGui::EndPopup(); break;
			case end_action::tab_bar: ImGui::EndTabBar(); break;
			case end_action::tab_item: ImGui::EndTabItem(); break;
			case end_action::table: ImGui::EndTable(); break;
			case end_action::tooltip: ImGui::EndTooltip(); break;
			case end_action::disabled: ImGui::EndDisabled(); break;
			case end_action::tree_node: ImGui::TreePop(); break;
			case end_action::none: break;
			}
		}
	}

	inline void imgui_context::window_scope::text(std::string_view value) const
	{
		if (!owner || !owner->context || !visible)
		{
			return;
		}

		owner->make_current();
		const char* begin = value.empty() ? "" : value.data();
		ImGui::TextUnformatted(begin, begin + value.size());
	}

	inline void imgui_context::window_scope::text(const char* value) const
	{
		if (!value) [[unlikely]]
		{
			throw std::invalid_argument("Dear ImGui text cannot be null");
		}
		text(std::string_view(value));
	}
} // namespace mngl
