#define APP_NAME "MNGL ImGui swapchain format regression test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/detail/imgui_compat/widgets.hpp>
#include <MNGL/render_window.hpp>
#include <MNGL/runtime.hpp>

#include <array>
#include <exception>
#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	void draw_imgui_content(const char* title, const char* text)
	{
		ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(280.0f, 160.0f), ImGuiCond_Always);
		if (ImGui::Begin(title))
		{
			ImGui::TextUnformatted(text);
		}
		ImGui::End();
	}

	void draw_overlay(mngl::render_window& window, mngl::imgui_context& gui)
	{
		window.clear();
		{
			auto frame = gui.frame();
			draw_imgui_content("Swapchain format regression", "The attached ImGui renderer remains active.");
		}
		const ImDrawData* data = ImGui::GetDrawData();
		require(data && data->TotalVtxCount > 0,
		        "The regression frame did not produce visible ImGui draw data");
		window.display();
	}

	void draw_offscreen(mngl::render_window& window, mngl::imgui_context& gui, mngl::texture& target)
	{
		{
			auto frame = gui.frame(target);
			window.clear(target);
			draw_imgui_content("Offscreen format regression", "The automatic workspace texture matches the window.");
		}
		const ImDrawData* data = ImGui::GetDrawData();
		require(data && data->TotalVtxCount > 0,
		        "The offscreen regression frame produced no ImGui draw data");
		window.display();
	}

	bool switch_to_a_different_format(mngl::render_window& window, mngl::gpu::texture_format initial_format)
	{
		constexpr std::array modes{mngl::gpu::device::hdr_mode::sdr_linear, mngl::gpu::device::hdr_mode::hdr,
		                           mngl::gpu::device::hdr_mode::hdr10};
		for (const auto mode : modes)
		{
			try
			{
				window.set_swapchain_parameters(mode, mngl::gpu::device::vsync_mode::vsync);
				if (window.swapchain_texture_format() != initial_format)
				{
					return true;
				}
			}
			catch (const mngl::sdl_error&)
			{
				// Try another SDL-supported composition on this adapter.
			}
		}
		return false;
	}

	void test_overlay_and_auto_texture_follow_swapchain_format()
	{
		mngl::window::flags flags;
		flags.hidden = true;
		mngl::render_window window("MNGL format regression", {320, 240}, flags);
		window.enable_hit_testing();
		mngl::imgui_context gui(window);
		ImGui::GetIO().IniFilename = nullptr;

		const auto initial_format = window.swapchain_texture_format();
		require(initial_format != mngl::gpu::texture_format::invalid,
		        "The test window did not expose a valid swapchain format");
		mngl::resizable_texture automatic_target(window);
		mngl::resizable_texture fixed_target(window, initial_format);
		require(automatic_target.target().format() == initial_format,
		        "The automatic target did not start with the initial swapchain format");
		require(fixed_target.target().format() == initial_format,
		        "The fixed-format target did not retain its requested format");

		draw_overlay(window, gui);
		ImGuiContext* const context_before_change = ImGui::GetCurrentContext();
		{
			auto frame = gui.frame();
			draw_imgui_content("Pending format transition",
			                   "This draw data was generated before the swapchain changes.");
		}
		const ImDrawData* pending_data = ImGui::GetDrawData();
		require(pending_data && pending_data->TotalVtxCount > 0,
		        "The pending transition frame did not produce visible ImGui draw data");
		if (!switch_to_a_different_format(window, initial_format))
		{
			throw mngl::sdl_error("This SDL GPU adapter exposes no alternate swapchain color format: ");
		}

		const auto changed_format = window.swapchain_texture_format();
		require(changed_format != initial_format,
		        "The selected swapchain composition did not change the render target "
		        "format");
		window.clear();
		window.display();
		require(ImGui::GetCurrentContext() == context_before_change,
		        "Reconfiguring the GPU backend replaced the Dear ImGui context");

		auto& automatic = automatic_target.target();
		require(automatic.format() == changed_format,
		        "The automatic resizable texture kept its old format after the "
		        "swapchain changed");
		require(fixed_target.target().format() == initial_format,
		        "An explicitly requested resizable-texture format changed with the "
		        "swapchain");

		draw_offscreen(window, gui, automatic);
		draw_overlay(window, gui);
		require(ImGui::GetCurrentContext() == context_before_change,
		        "Reconfiguring the GPU backend replaced the Dear ImGui context");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		test_overlay_and_auto_texture_follow_swapchain_format();
		return 0;
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "No supported alternate SDL GPU swapchain format on this runner: " << error.what()
		          << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << "ImGui swapchain format regression test failed: " << error.what() << '\n';
		return 1;
	}
}
