#include "input.hpp"
#include "imgui_compat.hpp"
#include "render_window.hpp"

#include <format>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

int main()
{
	mngl::window::flags window_flags;
	window_flags.resizeable = true;
	mngl::render_window window("MNGL ImGui integration check", { 800, 600 }, window_flags);
	auto workspace_texture = window.create_texture(window.pixel_size());
	auto workspace_size = window.coordinate_size();
	mngl::sprite workspace_sprite(
		workspace_texture,
		mngl::rectangle{ 0.0f, 0.0f, workspace_size.x, workspace_size.y });
	mngl::imgui_context gui(window);
	window.enable_hit_testing();
	auto panel = window.create_texture({ 400, 300 });
	panel.enable_hit_testing();
	panel.set_view(mngl::view{ mngl::rectangle{ 0.0f, 0.0f, 1.2f, 0.9f } });

	const mngl::rectangle bounds{ 0.2f, 0.2f, 0.8f, 0.5f };
	const mngl::rectangle panel_bounds{ 0.26666668f, 0.2f, 0.8f, 0.6f };
	mngl::sprite panel_sprite(panel, panel_bounds);
	mngl::two_d::transform panel_transform;
	panel_transform.rotate(0.12f, bounds.pos + bounds.size * 0.5f);
	mngl::two_d::rounded_rectangle button;
	button.bounds = bounds;
	button.radius = 0.08f;
	button.transform = panel_transform;
	int scene_button_clicks = 0;
	button.on_click([&](const mngl::hit_test_result&, const mngl::hit_test_path_entry&)
	{
		++scene_button_clicks;
	});
	bool has_completed_frame = false;
	bool show_workspace_surface = true;
	int mapping_mode = 4;
	int imgui_button_clicks = 0;
	int sample_count = 8;
	float ui_scale = 1.0f;
	double precise_value = 0.125;
	glm::vec2 world_position{ 0.2f, 0.5f };
	glm::vec3 world_direction{ 0.0f, 0.0f, 1.0f };
	glm::vec4 tint{ 1.0f, 0.5f, 0.25f, 1.0f };
	glm::ivec2 grid_position{ 2, 4 };
	float loading_progress = 0.68f;
	bool show_texture = true;
	bool selectable_item = false;
	int selected_quality = 1;
	bool show_controls = true;
	bool show_hierarchy = true;
	bool show_inspector = true;
	bool show_output = true;

	const std::vector<std::string> render_modes{ "Solid", "Wireframe", "Points" };
	const std::vector<int> frame_rates{ 30, 60, 120 };
	const std::vector<mngl::scancode> keys{
		mngl::scancode::a,
		mngl::scancode::b,
		mngl::scancode::escape
	};
	std::string username;
	const std::regex username_pattern(R"([A-Za-z0-9_]*)");

	while (mngl::Input::continue_work())
	{
		const auto target_size = window.pixel_size();
		if (target_size.x == 0 || target_size.y == 0)
		{
			continue;
		}
		if (workspace_texture.size() != target_size)
		{
			workspace_texture = window.create_texture(target_size);
		}
		workspace_size = window.coordinate_size();
		if (mngl::Input::is_key_pressed(mngl::scancode::f1))
		{
			show_workspace_surface = !show_workspace_surface;
		}

		{
			auto frame = gui.frame(workspace_texture);
			{
				auto menu_bar = frame.main_menu_bar();
				if (menu_bar)
				{
					auto view = menu_bar.menu("View");
					if (view)
					{
						(void)view.toggle("Controls", show_controls);
						(void)view.toggle("Hierarchy", show_hierarchy);
						(void)view.toggle("Inspector", show_inspector);
						(void)view.toggle("Output", show_output);
						(void)view.toggle("Workspace surface", show_workspace_surface);
					}
				}
			}
			auto workspace = frame.window("Workspace");
			auto docking = workspace.dockspace("MainWorkspace");
			{
				auto controls = docking.window("Controls###mngl.controls", show_controls);
				if (controls)
				{
					controls.text("Offscreen sprite hit-test modes:");
					(void)controls.radio_button(
						"Full window",
						mapping_mode,
						0);
					(void)controls.radio_button(
						"Translated + scaled",
						mapping_mode,
						1);
					(void)controls.radio_button(
						"Rotated",
						mapping_mode,
						2);
					(void)controls.radio_button(
						"UV cropped",
						mapping_mode,
						3);
					(void)controls.radio_button(
						"Combined transform + crop",
						mapping_mode,
						4);
					const auto mouse_position = mngl::Input::get_mouse_position();
					controls.text(std::format(
						"Host cursor: {:.1f}, {:.1f}",
						mouse_position.x,
						mouse_position.y));
					controls.text("Scroll this panel; edit Username; try buttons and sliders.");
					controls.text("Dear ImGui runs directly on the SDL_GPU backend.");
					if (controls.button("Click me"))
					{
						++imgui_button_clicks;
					}
					controls.text(std::format("Button clicks: {}", imgui_button_clicks));
					controls.progress_bar(loading_progress);
					controls.progress_bar(
						loading_progress,
						std::format("Loaded {:.0f}%", loading_progress * 100.0f));
					if (controls.selectable("Selectable row", selectable_item))
					{
						selectable_item = !selectable_item;
					}
					if (const auto action = controls.context_menu({
						std::string_view{ "Select" },
						std::string_view{ "Deselect" }
					}))
					{
						if (*action == "Select")
						{
							selectable_item = true;
						}
						else if (*action == "Deselect")
						{
							selectable_item = false;
						}
					}
					controls.bullet("Bullet with its own text");
					controls.bullet();
					controls.text("Text following a standalone bullet");
					(void)controls.radio_button("Low quality", selected_quality, 0);
					(void)controls.radio_button("Medium quality", selected_quality, 1);
					(void)controls.radio_button("High quality", selected_quality, 2);
					(void)controls.input("Sample count", sample_count);
					if (controls.button(sample_count))
					{
						++sample_count;
					}
					(void)controls.slider("Sample count range", sample_count, 0, 100);
					(void)controls.input("UI scale", ui_scale);
					(void)controls.slider("UI scale range", ui_scale, 0.25f, 2.0f);
					(void)controls.input("Precise value", precise_value);
					(void)controls.slider("Precise value range", precise_value, 0.0, 1.0);
					(void)controls.slider("World position", world_position, -1.0f, 1.0f);
					(void)controls.slider("World direction", world_direction, -1.0f, 1.0f);
					(void)controls.slider("Tint", tint, 0.0f, 1.0f);
					(void)controls.slider("Grid position", grid_position, -20, 20);
					(void)controls.checkbox("Show texture", show_texture);
					if (const auto mode = controls.combo("Render mode", render_modes))
					{
						controls.text(**mode);
					}
					if (const auto frame_rate = controls.combo("Frame rate", frame_rates))
					{
						controls.text(**frame_rate);
					}
					if (const auto key = controls.combo("Key name", keys))
					{
						controls.text(mngl::to_string(**key));
					}
					if (const auto quick_mode = controls.combo("Quick mode", { "A", "B" }))
					{
						controls.text(*quick_mode);
					}
					if (const auto quality = controls.combo(
						"Quality",
						std::initializer_list<std::string>{ "Low", "Medium", "High" }))
					{
						controls.text(*quality);
					}
					controls.text("Value output overloads:");
					controls.text(sample_count);
					controls.text(ui_scale);
					controls.text(precise_value);
					controls.text(show_texture);
					(void)controls.input("Username", username, username_pattern);
					controls.text(std::format("Username: {}", username));
					if (show_texture)
					{
						controls.image(panel, { 180.0f, 135.0f });
					}
				}
			}
			{
				auto hierarchy = docking.window("Hierarchy###mngl.hierarchy", show_hierarchy);
				if (hierarchy)
				{
					hierarchy.text("Workspace panes share this host window.");
					(void)hierarchy.selectable("World", selectable_item);
					(void)hierarchy.selectable("Camera");
					(void)hierarchy.selectable("Light");
				}
			}
			{
				auto inspector = docking.window("Inspector###mngl.inspector", show_inspector);
				if (inspector)
				{
					inspector.text("Selected object properties");
					inspector.text(std::format("Sample count: {}", sample_count));
					inspector.text(std::format("UI scale: {:.2f}", ui_scale));
					inspector.text(std::format("World position: {}, {}", world_position.x, world_position.y));
				}
			}
			{
				auto output = docking.window("Output###mngl.output", show_output);
				if (output)
				{
					output.text("Docked tabs can be rearranged by dragging them.");
					output.text("F1 hides/shows the UI sprite; hide it to test the red 2D button.");
					output.text("Resize the window to test offscreen target recreation.");
					output.text("Transparent pixels still hit the sprite; dragging outside may stop routing.");
					output.text(std::format("Button clicks: {}", imgui_button_clicks));
					output.text(std::format("2D scene button clicks: {}", scene_button_clicks));
				}
			}
		}

		glm::vec2 sprite_position{};
		glm::vec2 sprite_size = workspace_size;
		glm::vec4 sprite_uv{ 0.0f, 0.0f, 1.0f, 1.0f };
		float sprite_rotation = 0.0f;
		const glm::vec2 inset_position = workspace_size * 0.05f;
		const glm::vec2 inset_size = workspace_size * 0.90f;
		switch (mapping_mode)
		{
		case 0:
			break;
		case 1:
			sprite_position = inset_position;
			sprite_size = inset_size;
			break;
		case 2:
			sprite_position = inset_position;
			sprite_size = inset_size;
			sprite_rotation = 0.055f;
			break;
		case 3:
			sprite_position = inset_position;
			sprite_size = inset_size;
			sprite_uv = { 0.08f, 0.08f, 0.92f, 0.92f };
			break;
		case 4:
			sprite_position = inset_position;
			sprite_size = inset_size;
			sprite_rotation = 0.045f;
			sprite_uv = { 0.05f, 0.05f, 0.95f, 0.95f };
			break;
		default:
			mapping_mode = 4;
			break;
		}
		workspace_sprite
			.position(sprite_position)
			.size(sprite_size)
			.rotation(sprite_rotation);
		workspace_sprite.source.uv_bounds = sprite_uv;

		window.clear(workspace_texture, mngl::color{ 0.0f, 0.0f, 0.0f, 0.0f });
		window.display();

		if (has_completed_frame &&
			mngl::Input::is_mouse_button_pressed(mngl::mouse_button::left))
		{
			(void)window.dispatch_click(mngl::Input::get_mouse_position());
		}

		mngl::color fill{ 0.9f, 0.25f, 0.15f, 1.0f };
		if (has_completed_frame)
		{
			if (const auto hit = window.hit_test(mngl::Input::get_mouse_position());
				hit && hit->path.size() > 1)
			{
				const auto local = hit->path.back().local_coords;
				fill = {
					local.x / bounds.size.x,
					local.y / bounds.size.y,
					0.15f,
					1.0f
				};
			}
		}
		if (scene_button_clicks != 0)
		{
			fill = { 0.2f, 0.75f, 0.3f, 1.0f };
		}

		panel.clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f });
		panel.draw([&](mngl::two_d::Frame2D& frame)
		{
			button.tint = fill;
			frame.draw(button);
		});
		panel.display();

		window.clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f });
		window.draw(panel_sprite);
		if (show_workspace_surface)
		{
			window.draw(workspace_sprite);
		}
		window.display();
		has_completed_frame = true;
	}
}
