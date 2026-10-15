#include "input.hpp"
#include "imgui_compat.hpp"
#include "render_window.hpp"
#include "tree_practicum_ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

static glm::uvec2 drawable_pixel_size(mngl::window& window)
{
	constexpr SDL_WindowFlags unavailable_flags =
		SDL_WINDOW_HIDDEN |
		SDL_WINDOW_MINIMIZED |
		SDL_WINDOW_OCCLUDED;
	if ((SDL_GetWindowFlags(window.raw()) & unavailable_flags) != 0)
	{
		return {};
	}
	return window.pixel_size();
}

static int run_demo()
{
	if (SDL_Runtime.error())
	{
		throw std::runtime_error(SDL_Runtime.content());
	}

	SDL_Log("MNGL startup checkpoint: SDL initialized");
	mngl::window::flags window_flags;
	window_flags.fullscreen = true;
	window_flags.borderless = true;
	window_flags.high_pixel_density = true;

	float initial_scale = 1.0f;
	if (auto disp = SDL_GetPrimaryDisplay())
	{
		float s = SDL_GetDisplayContentScale(disp);
		if (s > 0.0f && std::isfinite(s)) initial_scale = s;
	}
	const glm::uvec2 base_size{ 960, 720 };
	const glm::uvec2 initial_size{
		static_cast<unsigned int>(std::max(1.0f, std::round(base_size.x * initial_scale))),
		static_cast<unsigned int>(std::max(1.0f, std::round(base_size.y * initial_scale)))
	};
	SDL_Log("MNGL startup checkpoint: creating render window");
	mngl::render_window window("MNGL — проверка ImGui", initial_size, window_flags);
	SDL_Log("MNGL startup checkpoint: render window and GPU device ready");
	auto workspace_texture = window.create_texture(window.pixel_size());
	SDL_Log("MNGL startup checkpoint: workspace texture ready");
	auto workspace_size = window.coordinate_size();
	mngl::sprite workspace_sprite(
		workspace_texture,
		mngl::rectangle{ 0.0f, 0.0f, workspace_size.x, workspace_size.y });
	const mngl::rectangle bounds{ 0.2f, 0.2f, 0.8f, 0.5f };
	const mngl::rectangle panel_bounds{ 0.26666668f, 0.2f, 0.8f, 0.6f };
	const auto panel_resolution = [panel_bounds](glm::uvec2 window_pixels)
	{
		const auto short_side = window_pixels.x < window_pixels.y
			? window_pixels.x
			: window_pixels.y;
		const glm::vec2 desired_size =
			panel_bounds.size * static_cast<float>(short_side);
		return glm::uvec2{
			static_cast<unsigned int>(std::max(1.0f, std::round(desired_size.x))),
			static_cast<unsigned int>(std::max(1.0f, std::round(desired_size.y)))
		};
	};
	const auto create_panel_texture = [&window](glm::uvec2 size)
	{
		auto result = window.create_texture(size);
		result.enable_hit_testing();
		result.set_view(mngl::view{ mngl::rectangle{ 0.0f, 0.0f, 1.2f, 0.9f } });
		return result;
	};
	window.enable_hit_testing();
	auto panel = create_panel_texture(panel_resolution(window.pixel_size()));

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

	const auto demo_font_path = []()
	{
		const std::filesystem::path candidates[]{
			"imgui/misc/fonts/Roboto-Medium.ttf",
			"MNGL/imgui/misc/fonts/Roboto-Medium.ttf",
			"../../MNGL/imgui/misc/fonts/Roboto-Medium.ttf"
		};
		for (const auto& candidate : candidates)
		{
			if (std::filesystem::is_regular_file(candidate))
			{
				return candidate;
			}
		}
		throw std::runtime_error("Could not locate the bundled Roboto font for the ImGui demo");
	};
	mngl::imgui_compat::font_config font_config;
	font_config.regular.file = demo_font_path();
	SDL_Log("MNGL startup checkpoint: creating ImGui context");
	mngl::imgui_context gui(window, font_config);
	SDL_Log("MNGL startup checkpoint: ImGui context ready");
	const std::vector<std::string> render_modes{ "Заливка", "Каркас", "Точки" };
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
		const auto target_size = drawable_pixel_size(window);
		if (target_size.x == 0 || target_size.y == 0)
		{
			SDL_Delay(10);
			continue;
		}
		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: entering first frame");
		}
		const auto logical_window_size = window.size();
		if (workspace_texture.size() != target_size)
		{
			workspace_texture = window.create_texture(target_size);
		}
		const auto next_panel_size = panel_resolution(target_size);
		if (panel.size() != next_panel_size)
		{
			panel = create_panel_texture(next_panel_size);
		}
		workspace_size = window.coordinate_size();
		if (mngl::Input::is_key_pressed(mngl::scancode::f1))
		{
			show_workspace_surface = !show_workspace_surface;
		}

		{
			gui.set_user_scale(ui_scale);
			if (!has_completed_frame)
			{
				SDL_Log("MNGL startup checkpoint: beginning ImGui frame");
			}
			auto frame = gui.frame(workspace_texture);
			{
				auto menu_bar = frame.main_menu_bar();
				if (menu_bar)
				{
					auto view = menu_bar.menu("Вид");
					if (view)
					{
						(void)view.toggle("Управление", show_controls);
						(void)view.toggle("Иерархия", show_hierarchy);
						(void)view.toggle("Инспектор", show_inspector);
						(void)view.toggle("Вывод", show_output);
						(void)view.toggle("Поверхность UI", show_workspace_surface);
					}
				}
			}
			auto workspace = frame.window("Рабочая область###Workspace");
			auto docking = workspace.dockspace("MainWorkspace");
			{
				auto controls = docking.window("Управление###mngl.controls", show_controls);
				if (controls)
				{
					controls.text("Режимы hit-test для offscreen-спрайта:");
					(void)controls.radio_button(
						"Окно целиком",
						mapping_mode,
						0);
					(void)controls.radio_button(
						"Смещение + масштаб",
						mapping_mode,
						1);
					(void)controls.radio_button(
						"Поворот",
						mapping_mode,
						2);
					(void)controls.radio_button(
						"Обрезка UV",
						mapping_mode,
						3);
					(void)controls.radio_button(
						"Поворот + обрезка UV",
						mapping_mode,
						4);
					const auto mouse_position = mngl::Input::get_mouse_position();
					controls.text(std::format(
						"Курсор окна: {:.1f}, {:.1f}",
						mouse_position.x,
						mouse_position.y));
					controls.text("Прокрути панель, попробуй кнопки, поля и ползунки :)");
					controls.text("Dear ImGui работает напрямую через SDL_GPU.");
					if (controls.button("Нажми меня :D"))
					{
						++imgui_button_clicks;
					}
					controls.text(std::format("Нажатия кнопки: {}", imgui_button_clicks));
					controls.progress_bar(loading_progress);
					controls.progress_bar(
						loading_progress,
						std::format("Загружено {:.0f}%", loading_progress * 100.0f));
					if (controls.selectable("Выбираемый пункт :)", selectable_item))
					{
						selectable_item = !selectable_item;
					}
					if (const auto action = controls.context_menu({
						std::string_view{ "Выбрать" },
						std::string_view{ "Снять выбор" }
					}))
					{
						if (*action == "Выбрать")
						{
							selectable_item = true;
						}
						else if (*action == "Снять выбор")
						{
							selectable_item = false;
						}
					}
					controls.bullet("Пункт списка со своим текстом");
					controls.bullet();
					controls.text("Текст после отдельного маркера");
					(void)controls.radio_button("Низкое качество", selected_quality, 0);
					(void)controls.radio_button("Среднее качество", selected_quality, 1);
					(void)controls.radio_button("Высокое качество", selected_quality, 2);
					(void)controls.input("Число образцов", sample_count);
					if (controls.button(sample_count))
					{
						++sample_count;
					}
					(void)controls.slider("Диапазон образцов", sample_count, 0, 100);
					(void)controls.input("Масштаб интерфейса", ui_scale);
					(void)controls.slider("Диапазон масштаба", ui_scale, 0.25f, 2.0f);
					(void)controls.input("Точное значение", precise_value);
					(void)controls.slider("Диапазон значения", precise_value, 0.0, 1.0);
					(void)controls.slider("Положение в мире", world_position, -1.0f, 1.0f);
					(void)controls.slider("Направление", world_direction, -1.0f, 1.0f);
					(void)controls.slider("Оттенок", tint, 0.0f, 1.0f);
					(void)controls.slider("Позиция сетки", grid_position, -20, 20);
					(void)controls.checkbox("Показывать текстуру", show_texture);
					if (const auto mode = controls.combo("Режим отрисовки", render_modes))
					{
						controls.text(**mode);
					}
					if (const auto frame_rate = controls.combo("Частота кадров", frame_rates))
					{
						controls.text(**frame_rate);
					}
					if (const auto key = controls.combo("Клавиша", keys))
					{
						controls.text(mngl::to_string(**key));
					}
					if (const auto quick_mode = controls.combo("Быстрый режим", { "A", "B" }))
					{
						controls.text(*quick_mode);
					}
					if (const auto quality = controls.combo(
						"Качество",
						std::initializer_list<std::string>{ "Низкое", "Среднее", "Высокое" }))
					{
						controls.text(*quality);
					}
					controls.text("Перегрузки вывода значений:");
					controls.text(sample_count);
					controls.text(ui_scale);
					controls.text(precise_value);
					controls.text(show_texture);
					(void)controls.input("Имя пользователя (латиница)", username, username_pattern);
					controls.text(std::format("Имя пользователя: {}", username));
					if (show_texture)
					{
						const float eff_scale = gui.effective_scale();
						controls.image(panel, { 180.0f * eff_scale, 135.0f * eff_scale });
					}
				}
			}
			{
				auto hierarchy = docking.window("Иерархия###mngl.hierarchy", show_hierarchy);
				if (hierarchy)
				{
					hierarchy.text("Все панели рабочего пространства в одном окне.");
					(void)hierarchy.selectable("Мир", selectable_item);
					(void)hierarchy.selectable("Камера");
					(void)hierarchy.selectable("Источник света");
				}
			}
			{
				auto inspector = docking.window("Инспектор###mngl.inspector", show_inspector);
				if (inspector)
				{
					inspector.text("Параметры выбранного объекта");
					inspector.text(std::format("Число образцов: {}", sample_count));
					inspector.text(std::format("Масштаб UI: {:.2f}", ui_scale));
					inspector.text(std::format(
						"Положение: {}, {}",
						world_position.x,
						world_position.y));
				}
			}
			{
				auto output = docking.window("Вывод###mngl.output", show_output);
				if (output)
				{
					output.text("Вкладки можно переставлять и делить перетаскиванием :)");
					output.text("F1 скрывает UI-спрайт: за ним можно нажать красную 2D-кнопку.");
					output.text("Измени размер окна: обе offscreen-текстуры подстроятся.");
					output.text("Перетаскивание UI продолжается за границами поверхности.");
					output.text(std::format(
						"Окно: {}x{} пикс. (Масштаб экрана: {:.2f}, UI: {:.2f})",
						target_size.x,
						target_size.y,
						window.display_scale(),
						gui.effective_scale()));
					output.text(std::format(
						"Текстуры: UI {}x{}; сцена {}x{} пикс.",
						workspace_texture.width(),
						workspace_texture.height(),
						panel.width(),
						panel.height()));
					output.text(std::format("Нажатия ImGui: {}", imgui_button_clicks));
					output.text(std::format("Нажатия 2D-кнопки: {}", scene_button_clicks));
				}
			}
		}
		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: ImGui frame built");
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

		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: rendering workspace texture");
		}
		window.clear(workspace_texture, mngl::color{ 0.0f, 0.0f, 0.0f, 0.0f });
		window.display();
		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: workspace texture rendered");
		}

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

		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: rendering panel texture");
		}
		panel.clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f });
		panel.draw([&](mngl::two_d::Frame2D& frame)
		{
			button.tint = fill;
			frame.draw(button);
		});
		panel.display();
		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: panel texture rendered");
		}

		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: rendering swapchain");
		}
		if (!window.try_clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f }))
		{
			SDL_Delay(10);
			continue;
		}
		window.draw(panel_sprite);
		if (show_workspace_surface)
		{
			window.draw(workspace_sprite);
		}
		if (!window.try_display())
		{
			SDL_Delay(10);
			continue;
		}
		if (!has_completed_frame)
		{
			SDL_Log("MNGL startup checkpoint: first frame presented");
		}
		has_completed_frame = true;
	}
	return 0;
}

static std::filesystem::path find_font_file(std::string_view file_name)
{
	const std::array<std::filesystem::path, 3> directories{
		"imgui/misc/fonts",
		"MNGL/imgui/misc/fonts",
		"../../MNGL/imgui/misc/fonts"
	};
	for (const auto& directory : directories)
	{
		const auto candidate =
			directory / std::filesystem::path(std::string(file_name));
		if (std::filesystem::is_regular_file(candidate))
		{
			return candidate;
		}
	}
	return {};
}

static int run_tree_practicum()
{
	if (SDL_Runtime.error())
	{
		throw std::runtime_error(SDL_Runtime.content());
	}

	mngl::window::flags window_flags;
	window_flags.fullscreen = true;
	window_flags.borderless = true;
	window_flags.high_pixel_density = true;

	float initial_scale = 1.0f;
	if (auto display = SDL_GetPrimaryDisplay())
	{
		const float display_scale = SDL_GetDisplayContentScale(display);
		if (display_scale > 0.0f && std::isfinite(display_scale))
		{
			initial_scale = display_scale;
		}
	}

	const glm::uvec2 base_size{ 1440, 900 };
	const glm::uvec2 initial_size{
		static_cast<unsigned int>(
			std::max(1.0f, std::round(base_size.x * initial_scale))),
		static_cast<unsigned int>(
			std::max(1.0f, std::round(base_size.y * initial_scale)))
	};
	mngl::render_window window(
		"Планировщик деревьев",
		initial_size,
		window_flags);

	mngl::imgui_compat::font_config font_config;
	font_config.regular.file = find_font_file("Roboto-Medium.ttf");
	font_config.monospace.file = find_font_file("Cousine-Regular.ttf");
	mngl::imgui_context gui(window, font_config);
	mngl::two_d::font tree_font(
		window.gpu_device(),
		find_font_file("Cousine-Regular.ttf"));
	tree_practicum::tree_practicum_ui ui(
		window,
		tree_font,
		window.raw());

	while (mngl::Input::continue_work())
	{
		const auto target_size = drawable_pixel_size(window);
		if (target_size.x == 0 || target_size.y == 0)
		{
			SDL_Delay(10);
			continue;
		}
		{
			auto frame = gui.frame();
			ui.draw(frame);
		}
		if (!window.try_clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f }))
		{
			SDL_Delay(10);
			continue;
		}
		if (!window.try_display())
		{
			SDL_Delay(10);
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	try
	{
		if (argc > 1 && std::string_view(argv[1]) == "--mngl-demo")
		{
			return run_demo();
		}
		return run_tree_practicum();
	}
	catch (const std::exception& error)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "MNGL unhandled exception: %s", error.what());
		return 1;
	}
	catch (...)
	{
		SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "MNGL unhandled non-standard exception");
		return 1;
	}
}
