#include "window.hpp"
#include "input.hpp"
#include "renderer2d.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>

int main()
{
	mngl::window win("hello, world!", {800, 600});
	mngl::gpu::device gpu;
	gpu.claim_window(win);

	const std::array<float, 4> storage_color{ 0.1f, 0.9f, 0.8f, 1.0f };
	mngl::gpu::command_buffer storage_upload(gpu);
	auto storage_color_buffer = mngl::gpu::buffer::create_with_data(
		gpu.raw(),
		storage_upload,
		std::span<const float>(storage_color),
		mngl::gpu::buffer_usage::graphics_storage_read);
	storage_upload.submit();

	mngl::two_d::texture_store textures(gpu);
	mngl::two_d::renderer renderer(gpu, win, textures);

	const char* executable_directory = SDL_GetBasePath();
	if (!executable_directory)
	{
		throw sdl_error("failed to get the executable directory: ");
	}
	mngl::gpu::graphics_shader_asset solid_fill_shader{};
	solid_fill_shader.stage = mngl::gpu::graphics_shader_stage::fragment;
	solid_fill_shader.code.assign(
		mngl::gpu::shader_format::dxil,
		read_binary_file(std::filesystem::path(executable_directory) / "shaders" / "custom_fill.frag.dxil"));
	solid_fill_shader.resources.uniform_buffers = 1;
	solid_fill_shader.debug_name = "custom_fill.frag";
	const auto solid_fill_material = renderer.create_material(
		solid_fill_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	mngl::gpu::fragment_resource_bindings solid_fill_bindings;
	const std::array<float, 4> purple{ 0.75f, 0.2f, 0.9f, 1.0f };
	solid_fill_bindings.add_uniform_block(purple);
	const auto solid_fill_instance = renderer.create_material_instance(
		solid_fill_material,
		std::move(solid_fill_bindings));

	mngl::gpu::graphics_shader_asset storage_fill_shader{};
	storage_fill_shader.stage = mngl::gpu::graphics_shader_stage::fragment;
	storage_fill_shader.code.assign(
		mngl::gpu::shader_format::dxil,
		read_binary_file(std::filesystem::path(executable_directory) / "shaders" / "custom_storage.frag.dxil"));
	storage_fill_shader.resources.storage_buffers = 1;
	storage_fill_shader.debug_name = "custom_storage.frag";
	const auto storage_fill_material = renderer.create_material(
		storage_fill_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	mngl::gpu::fragment_resource_bindings storage_fill_bindings;
	storage_fill_bindings.add_storage_buffer(storage_color_buffer);
	const auto storage_fill_instance = renderer.create_material_instance(
		storage_fill_material,
		std::move(storage_fill_bindings));

	std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> storage_surface(
		SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32),
		&SDL_DestroySurface);
	if (!storage_surface)
	{
		throw sdl_error("failed to create the storage texture demo surface: ");
	}
	const std::array<Uint8, 4> storage_pixel{ 40, 210, 255, 255 };
	std::memcpy(storage_surface->pixels, storage_pixel.data(), storage_pixel.size());
	mngl::gpu::command_buffer texture_upload(gpu);
	mngl::texture storage_texture(
		gpu,
		texture_upload,
		storage_surface.get(),
		mngl::texture::usage::graphics_read);
	texture_upload.submit();
	const auto storage_texture_handle = textures.add(std::move(storage_texture));

	mngl::gpu::graphics_shader_asset storage_texture_shader{};
	storage_texture_shader.stage = mngl::gpu::graphics_shader_stage::fragment;
	storage_texture_shader.code.assign(
		mngl::gpu::shader_format::dxil,
		read_binary_file(std::filesystem::path(executable_directory) / "shaders" / "custom_storage_texture.frag.dxil"));
	storage_texture_shader.resources.storage_textures = 1;
	storage_texture_shader.debug_name = "custom_storage_texture.frag";
	const auto storage_texture_material = renderer.create_material(
		storage_texture_shader,
		mngl::gpu::sprite_blend_mode::opaque);
	mngl::gpu::fragment_resource_bindings storage_texture_bindings;
	storage_texture_bindings.add_storage_texture(
		textures.resolve(storage_texture_handle).raw_gpu_texture());
	const auto storage_texture_instance = renderer.create_material_instance(
		storage_texture_material,
		std::move(storage_texture_bindings));

	const mngl::rectangle red_rect{ 80.0f, 80.0f, 180.0f, 120.0f };
	const mngl::rectangle green_rect{ 300.0f, 140.0f, 180.0f, 120.0f };
	const mngl::rectangle blue_rect{ 520.0f, 200.0f, 180.0f, 120.0f };
	const mngl::rectangle custom_rect{ 80.0f, 280.0f, 180.0f, 100.0f };
	const mngl::rectangle storage_rect{ 520.0f, 360.0f, 180.0f, 100.0f };
	const mngl::rectangle storage_texture_rect{ 300.0f, 300.0f, 180.0f, 100.0f };
	const mngl::rectangle rounded_rect_bounds{ 300.0f, 300.0f, 150.0f, 90.0f };
	const mngl::color red{ 1.0f, 0.2f, 0.2f, 1.0f };
	const mngl::color green{ 0.2f, 1.0f, 0.2f, 1.0f };
	const mngl::color blue{ 0.2f, 0.4f, 1.0f, 1.0f };
	const mngl::color yellow{ 1.0f, 0.8f, 0.1f, 1.0f };
	const auto white_region = textures.white_region();

	mngl::gpu::quad_transform red_transform{};
	red_transform.destination = red_rect;
	mngl::gpu::quad_transform green_transform{};
	green_transform.destination = green_rect;
	mngl::gpu::quad_transform blue_transform{};
	blue_transform.destination = blue_rect;
	mngl::two_d::sprite red_sprite{ white_region, red_transform, red };
	mngl::two_d::sprite green_sprite{ white_region, green_transform, green };
	mngl::two_d::sprite blue_sprite{ white_region, blue_transform, blue };
	mngl::two_d::sprite custom_fill_sprite{};
	custom_fill_sprite.transform.destination = custom_rect;
	custom_fill_sprite.state.material = solid_fill_instance;
	mngl::two_d::sprite storage_fill_sprite{};
	storage_fill_sprite.transform.destination = storage_rect;
	storage_fill_sprite.state.material = storage_fill_instance;
	mngl::two_d::sprite storage_texture_sprite{};
	storage_texture_sprite.transform.destination = storage_texture_rect;
	storage_texture_sprite.state.material = storage_texture_instance;
	const mngl::two_d::circle yellow_circle{ { 400.0f, 410.0f }, 50.0f, yellow };
	const mngl::two_d::ellipse cyan_ellipse{ { 220.0f, 475.0f }, { 70.0f, 35.0f }, mngl::color(0.1f, 0.8f, 0.9f, 1.0f) };
	const mngl::two_d::rounded_rectangle soft_rectangle{
		rounded_rect_bounds,
		20.0f,
		mngl::color(0.9f, 0.5f, 0.1f, 1.0f)
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

		auto frame = renderer.begin_frame();
		frame.draw(red_sprite);
		frame.draw(green_sprite);
		frame.draw(blue_sprite);
		frame.draw(custom_fill_sprite);
		frame.draw(storage_fill_sprite);
		frame.draw(storage_texture_sprite);
		frame.draw(yellow_circle);
		frame.draw(cyan_ellipse);
		frame.draw(soft_rectangle);
		frame.finish();
	}
}