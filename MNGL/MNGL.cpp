#include "input.hpp"
#include "render_window.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>

int main()
{
	mngl::render_window win("hello, world!", {800, 600});
	auto& gpu = win.gpu_device();
	auto& textures = win.textures();
	auto& renderer = win.renderer();

	const std::array<float, 4> storage_color{ 0.1f, 0.9f, 0.8f, 1.0f };
	mngl::gpu::command_buffer storage_upload(gpu);
	auto storage_color_buffer = mngl::gpu::buffer::create_with_data(
		gpu.raw(),
		storage_upload,
		std::span<const float>(storage_color),
		mngl::gpu::buffer_usage::graphics_storage_read);
	storage_upload.submit();

	const char* executable_directory = SDL_GetBasePath();
	if (!executable_directory)
	{
		throw sdl_error("failed to get the executable directory: ");
	}
	const auto shader_directory = std::filesystem::path(executable_directory) / "shaders";
	const auto load_fragment_shader = [&](std::string_view name)
	{
		mngl::gpu::shader_compile_options options{};
		options.stage = mngl::gpu::graphics_shader_stage::fragment;
		options.format = mngl::gpu::shader_format::dxil;
		options.reflect_uniforms = true;
		return mngl::gpu::shader_loader::load_binary(
			shader_directory / (std::string(name) + ".frag.dxil"),
			shader_directory / (std::string(name) + ".frag.hlsl"),
			options);
	};

	auto solid_fill_shader = load_fragment_shader("custom_fill");
	const mngl::color purple{ 0.75f, 0.2f, 0.9f, 1.0f };
	solid_fill_shader[0] = purple;
	const auto solid_fill_material = renderer.create_material(
		solid_fill_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	const auto solid_fill_instance = renderer.create_material_instance_from_shader(
		solid_fill_material,
		solid_fill_shader);

	auto storage_fill_shader = load_fragment_shader("custom_storage");
	storage_fill_shader["storage_color"] = storage_color_buffer;
	const auto storage_fill_material = renderer.create_material(
		storage_fill_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	const auto storage_fill_instance = renderer.create_material_instance_from_shader(
		storage_fill_material,
		storage_fill_shader);

	const std::array<Uint8, 4> storage_pixel{ 40, 210, 255, 255 };
	mngl::texture storage_texture(
		1,
		1,
		mngl::gpu::texture_format::r8g8b8a8_unorm,
		mngl::texture::usage::graphics_read);
	mngl::gpu::command_buffer texture_upload(gpu);
	storage_texture.update_pixels(texture_upload, std::span<const Uint8>(storage_pixel));
	texture_upload.submit();
	const auto storage_texture_handle = textures.add(std::move(storage_texture));

	auto storage_texture_shader = load_fragment_shader("custom_storage_texture");
	storage_texture_shader["storage_texture"] = textures.resolve(storage_texture_handle);
	const auto storage_texture_material = renderer.create_material(
		storage_texture_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	const auto storage_texture_instance = renderer.create_material_instance_from_shader(
		storage_texture_material,
		storage_texture_shader);

	auto sampled_texture_shader = load_fragment_shader("batcher2d");
	sampled_texture_shader[0] = glm::uvec4(0u);
	sampled_texture_shader["sprite_texture"] =
		textures.resolve(textures.white_region().page);
	const auto sampled_texture_material = renderer.create_material(
		sampled_texture_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	const auto sampled_texture_instance = renderer.create_material_instance_from_shader(
		sampled_texture_material,
		sampled_texture_shader);

	auto point_sampler = mngl::gpu::sampler_builder::create_point_clamp(gpu.raw());
	auto explicit_sampled_shader = sampled_texture_shader;
	explicit_sampled_shader["sprite_texture"] = mngl::gpu::texture_binding{
		textures.resolve(textures.white_region().page),
		point_sampler
	};
	const auto explicit_sampled_material = renderer.create_material(
		explicit_sampled_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	const auto explicit_sampled_instance = renderer.create_material_instance_from_shader(
		explicit_sampled_material,
		explicit_sampled_shader);

	const mngl::rectangle red_rect{ 0.08f, 0.10f, 0.28f, 0.20f };
	const mngl::rectangle green_rect{ 0.50f, 0.22f, 0.28f, 0.20f };
	const mngl::rectangle blue_rect{ 0.95f, 0.34f, 0.28f, 0.20f };
	const mngl::rectangle custom_rect{ 0.08f, 0.36f, 0.28f, 0.17f };
	const mngl::rectangle storage_rect{ 0.95f, 0.62f, 0.28f, 0.16f };
	const mngl::rectangle storage_texture_rect{ 0.50f, 0.55f, 0.28f, 0.17f };
	const mngl::rectangle rounded_rect_bounds{ 0.50f, 0.35f, 0.24f, 0.15f };
	const mngl::color red{ 1.0f, 0.2f, 0.2f, 1.0f };
	const mngl::color green{ 0.2f, 1.0f, 0.2f, 1.0f };
	const mngl::color blue{ 0.2f, 0.4f, 1.0f, 1.0f };
	const mngl::color yellow{ 1.0f, 0.8f, 0.1f, 1.0f };


	mngl::sprite custom_fill_sprite{};
	custom_fill_sprite.transform.destination = custom_rect;
	custom_fill_sprite.state.material = solid_fill_instance;
	mngl::sprite storage_fill_sprite{};
	storage_fill_sprite.transform.destination = storage_rect;
	storage_fill_sprite.state.material = storage_fill_instance;
	mngl::sprite storage_texture_sprite{};
	storage_texture_sprite.transform.destination = storage_texture_rect;
	storage_texture_sprite.state.material = storage_texture_instance;
	mngl::sprite sampled_texture_sprite{};
	sampled_texture_sprite.transform.destination = mngl::rectangle{ 0.08f, 0.70f, 0.18f, 0.12f };
	sampled_texture_sprite.state.material = sampled_texture_instance;
	const mngl::two_d::circle yellow_circle{ { 0.67f, 0.70f }, 0.08f, yellow };
	const mngl::two_d::ellipse cyan_ellipse{
		{ 0.36f, 0.82f },
		{ 0.12f, 0.06f },
		mngl::color(0.1f, 0.8f, 0.9f, 1.0f)
	};
	const mngl::two_d::rounded_rectangle soft_rectangle{
		rounded_rect_bounds,
		0.04f,
		mngl::color(0.9f, 0.5f, 0.1f, 1.0f)
	};
	mngl::texture scene_target(800, 600);
	mngl::sprite scene_sprite{
		scene_target,
		mngl::rectangle{ 0.0f, 0.0f, 4.0f / 3.0f, 1.0f }
	};

	while (mngl::Input::continue_work())
	{
		if (mngl::Input::is_key_released(mngl::scancode::a))
		{
			win.borderless(false);
		}
		if (mngl::Input::is_key_released(mngl::scancode::b))
		{
			win.borderless(true);
		}

		scene_target.clear();
		scene_target.draw(red_rect, red);
		scene_target.draw(green_rect, green);
		scene_target.draw(blue_rect, blue);
		scene_target.draw(yellow_circle);
		scene_target.draw(cyan_ellipse);
		scene_target.draw(soft_rectangle);
		scene_target.display();

		win.clear();
		const auto window_coordinate_size = win.coordinate_size();
		const auto scene_coordinate_size = scene_target.coordinate_size();
		const float scene_scale = std::min(
			window_coordinate_size.x / scene_coordinate_size.x,
			window_coordinate_size.y / scene_coordinate_size.y);
		const auto fitted_scene_size = scene_coordinate_size * scene_scale;
		scene_sprite.size(fitted_scene_size);
		scene_sprite.position((window_coordinate_size - fitted_scene_size) * 0.5f);
		win.draw(scene_sprite);
		win.draw(custom_fill_sprite);
		win.draw(storage_fill_sprite);
		win.draw(storage_texture_sprite);
		win.draw(sampled_texture_sprite);
		win.display();
	}

	renderer.destroy_material_instance(explicit_sampled_instance);
	renderer.destroy_material(explicit_sampled_material);
}