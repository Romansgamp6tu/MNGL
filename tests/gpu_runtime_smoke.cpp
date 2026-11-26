#define APP_NAME "MNGL GPU runtime smoke"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/device.hpp>
#include <MNGL/detail/two_d/svg_effect_renderer.hpp>
#include <MNGL/frame_graph.hpp>
#include <MNGL/renderer2d.hpp>
#include <MNGL/gpu_buffer.hpp>
#include <MNGL/texture.hpp>
#include <MNGL/render_pass.hpp>

#include <array>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	void frame_graph_order_cache_invalidation(const mngl::gpu::device& device)
	{
		mngl::gpu::frame_graph graph(device);
		std::vector<int> order;
		auto reader = graph.add_pass("reader", [&](mngl::gpu::frame_graph::pass_context&) { order.push_back(0); });
		auto writer = graph.add_pass("writer", [&](mngl::gpu::frame_graph::pass_context&) { order.push_back(1); });
		const auto transient = graph.create_buffer(
		    16, mngl::gpu::buffer_usage::compute_storage_read | mngl::gpu::buffer_usage::compute_storage_write);

		graph.execute();
		require(order == std::vector<int>{0, 1}, "An unchanged frame graph must keep deterministic insertion order");

		order.clear();
		reader.read(transient);
		writer.write(transient);
		graph.execute();
		require(order == std::vector<int>{1, 0},
		        "Adding transient resource uses must invalidate the cached order and schedule the writer first");

		order.clear();
		auto tail = graph.add_pass("tail", [&](mngl::gpu::frame_graph::pass_context&) { order.push_back(2); });
		graph.execute();
		require(order == std::vector<int>{1, 0, 2}, "Adding a pass must invalidate the cached frame-graph order");

		order.clear();
		writer.depends_on(tail.handle());
		graph.execute();
		require(order == std::vector<int>{2, 1, 0},
		        "Adding an explicit dependency must invalidate and rebuild the cached frame-graph order");

		order.clear();
		graph.execute();
		require(order == std::vector<int>{2, 1, 0}, "Repeated execution must reuse the validated deterministic order");
	}

	void cached_layer_fragmentation_is_bounded()
	{
		mngl::two_d::cached_layer layer(glm::uvec2{128, 128});
		std::size_t callback_count = 0;
		mngl::rectangle last_region{};
		const auto draw_region = [&](mngl::two_d::frame_2d&, const mngl::rectangle& region)
		{
			++callback_count;
			last_region = region;
		};

		require(layer.update_dirty_regions(draw_region), "A new cached layer must perform its initial full redraw");
		require(callback_count == 1, "Initial cached-layer redraw must invoke one callback");
		callback_count = 0;

		const auto invalidate_fragments = [&](std::size_t count)
		{
			for (std::size_t index = 0; index < count; ++index)
			{
				const auto x = static_cast<float>((index % 8) * 3) / 128.0f;
				const auto y = static_cast<float>((index / 8) * 3) / 128.0f;
				layer.invalidate_region(mngl::rectangle{x, y, 1.0f / 128.0f, 1.0f / 128.0f});
			}
		};

		invalidate_fragments(63);
		require(layer.dirty(), "Fragmented cached-layer invalidations must remain pending until updated");
		require(layer.update_dirty_regions(draw_region), "Dirty cached-layer regions must be rendered");
		require(callback_count == 63, "The bounded region budget must preserve at most 63 separate callbacks");
		callback_count = 0;

		invalidate_fragments(64);
		require(layer.dirty(), "Excessively fragmented cached-layer invalidations must remain dirty");
		require(layer.update_dirty_regions(draw_region), "Excessive fragmentation must still trigger a redraw");
		require(callback_count == 1 && last_region.pos.x == 0.0f && last_region.pos.y == 0.0f &&
		            last_region.size.x == 1.0f && last_region.size.y == 1.0f,
		        "The 64th disjoint region must collapse to one full-layer redraw callback");
		require(!layer.dirty(), "A successful full redraw must clear cached-layer dirty state");
	}
	void test_partial_buffer_updates(const mngl::gpu::device& device)
	{
		const std::array<std::uint32_t, 4> initial{10, 20, 30, 40};
		const std::array<std::uint32_t, 1> first_patch{99};
		const std::array<std::uint32_t, 1> second_patch{77};
		constexpr auto snapshot_bytes = static_cast<std::uint32_t>(sizeof(initial));
		SDL_GPUTransferBufferCreateInfo info{};
		info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		info.size = snapshot_bytes * 3;
		auto* download = SDL_CreateGPUTransferBuffer(device.raw(), &info);
		mngl::check_sdl(download);
		mngl::detail::transfer_buffer_guard download_guard(device.raw(), download);

		mngl::gpu::command_buffer commands(device);
		auto buffer = mngl::gpu::buffer::create_with_data(
		    device, commands, std::span<const std::uint32_t>(initial), mngl::gpu::buffer_usage::vertex);
		const auto snapshot = [&](std::uint32_t index)
		{
			mngl::gpu::detail::gpu_copy_pass pass(commands);
			const SDL_GPUBufferRegion source{.buffer = buffer.raw(), .offset = 0, .size = snapshot_bytes};
			const SDL_GPUTransferBufferLocation destination{.transfer_buffer = download,
			                                                .offset = index * snapshot_bytes};
			SDL_DownloadFromGPUBuffer(pass.raw(), &source, &destination);
		};
		snapshot(0);
		buffer.upload_range(device, commands, 1, std::span<const std::uint32_t>(first_patch));
		snapshot(1);
		buffer.upload_range(device, commands, 3, std::span<const std::uint32_t>(second_patch));
		snapshot(2);
		commands.submit();
		mngl::check_sdl(SDL_WaitForGPUIdle(device.raw()));

		const void* mapped = SDL_MapGPUTransferBuffer(device.raw(), download, false);
		mngl::check_sdl(mapped);
		std::array<std::array<std::uint32_t, 4>, 3> actual{};
		std::memcpy(actual.data(), mapped, sizeof(actual));
		SDL_UnmapGPUTransferBuffer(device.raw(), download);
		require(actual[0] == initial, "Partial buffer update changed an earlier GPU command's data");
		require(actual[1] == std::array<std::uint32_t, 4>{10, 99, 30, 40},
		        "Partial buffer update discarded untouched values");
		require(actual[2] == std::array<std::uint32_t, 4>{10, 99, 30, 77},
		        "Repeated partial buffer update discarded previous updates");
	}

	void test_texture_usage(const mngl::gpu::device& device)
	{
		using usage = mngl::gpu::texture::usage_flags;
		const mngl::gpu::texture sampled(device, {2, 2}, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                                 usage::sampler);
		require(sampled.has_usage(usage::sampler), "Texture lost its sampler usage");
		require(!sampled.has_usage(usage::sampler | usage::color_target),
		        "Texture usage query accepted a missing requested flag");
	}

	void test_render_pass_uniforms(const mngl::gpu::device& device)
	{
		const std::array<float, 4> uniform{1.0f, 2.0f, 3.0f, 4.0f};
		const auto rejects_inactive = [&](const mngl::gpu::render_pass& pass)
		{
			bool vertex_rejected = false;
			bool fragment_rejected = false;
			try
			{
				pass.push_vertex_uniform(0, uniform);
			}
			catch (const std::logic_error&)
			{
				vertex_rejected = true;
			}
			try
			{
				pass.push_fragment_uniform(0, uniform);
			}
			catch (const std::logic_error&)
			{
				fragment_rejected = true;
			}
			require(vertex_rejected && fragment_rejected, "Inactive render pass accepted uniform data");
		};
		rejects_inactive(mngl::gpu::render_pass{});
		mngl::gpu::texture target(device, {2, 2}, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                          mngl::gpu::texture::usage_flags::color_target);
		mngl::gpu::command_buffer commands(device);
		mngl::gpu::render_pass pass(commands, target);
		pass.push_vertex_uniform(0, uniform);
		pass.push_fragment_uniform(0, uniform);
		mngl::gpu::render_pass moved(std::move(pass));
		rejects_inactive(pass);
		moved.push_vertex_uniform(0, uniform);
		moved.push_fragment_uniform(0, uniform);
		mngl::gpu::render_pass assigned;
		assigned = std::move(moved);
		rejects_inactive(moved);
		assigned.push_vertex_uniform(0, uniform);
		assigned.push_fragment_uniform(0, uniform);
		assigned.end_pass();
		rejects_inactive(assigned);
		commands.submit();

		mngl::gpu::command_buffer invalid(nullptr);
		bool invalid_rejected = false;
		try
		{
			mngl::gpu::render_pass unused(invalid, target);
		}
		catch (const std::invalid_argument&)
		{
			invalid_rejected = true;
		}
		require(invalid_rejected, "Render pass accepted an inactive command buffer");

		const mngl::gpu::device other_device;
		mngl::gpu::command_buffer foreign_commands(other_device);
		bool foreign_rejected = false;
		try
		{
			mngl::gpu::render_pass unused(foreign_commands, target);
		}
		catch (const std::invalid_argument&)
		{
			foreign_rejected = true;
		}
		require(foreign_rejected, "Render pass accepted a target from another GPU device");
	}

	void test_texture_update_preserves_pending_reads(const mngl::gpu::device& device)
	{
		std::array<std::uint32_t, 64> initial;
		initial.fill(0xFF123456u);
		std::array<std::uint32_t, 64> replacement;
		replacement.fill(0xFFABCDEFu);
		mngl::texture image(device, 64, 1, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                    mngl::texture::usage::sampler);
		mngl::gpu::command_buffer initialize(device);
		image.update_pixels(initialize, std::span<const std::uint32_t>(initial));
		initialize.submit();
		mngl::check_sdl(SDL_WaitForGPUIdle(device.raw()));

		// Use a 256-byte row and 512-byte offsets for native D3D12 readback alignment.
		SDL_GPUTransferBufferCreateInfo info{};
		info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		info.size = 1024;
		auto* download = SDL_CreateGPUTransferBuffer(device.raw(), &info);
		mngl::check_sdl(download);
		mngl::detail::transfer_buffer_guard download_guard(device.raw(), download);
		const auto snapshot = [&](mngl::gpu::command_buffer& commands, std::uint32_t offset)
		{
			mngl::gpu::detail::gpu_copy_pass pass(commands);
			SDL_GPUTextureRegion source{};
			source.texture = image.raw_gpu_texture().raw();
			source.w = 64;
			source.h = 1;
			source.d = 1;
			SDL_GPUTextureTransferInfo destination{};
			destination.transfer_buffer = download;
			destination.offset = offset;
			destination.pixels_per_row = 64;
			destination.rows_per_layer = 1;
			SDL_DownloadFromGPUTexture(pass.raw(), &source, &destination);
		};

		mngl::gpu::command_buffer pending_read(device);
		snapshot(pending_read, 0);
		mngl::gpu::command_buffer update(device);
		image.update_pixels(update, std::span<const std::uint32_t>(replacement));
		snapshot(update, 512);
		update.submit();
		pending_read.submit();
		mngl::check_sdl(SDL_WaitForGPUIdle(device.raw()));

		const void* mapped = SDL_MapGPUTransferBuffer(device.raw(), download, false);
		mngl::check_sdl(mapped);
		std::array<std::uint32_t, 64> earlier{};
		std::array<std::uint32_t, 64> later{};
		std::memcpy(earlier.data(), mapped, sizeof(earlier));
		std::memcpy(later.data(), static_cast<const std::byte*>(mapped) + 512, sizeof(later));
		SDL_UnmapGPUTransferBuffer(device.raw(), download);
		require(earlier == initial, "Texture update overwrote an image used by a pending GPU command");
		require(later == replacement, "Texture update did not upload the replacement pixels");
	}
} // namespace

int main()
{
	std::optional<mngl::gpu::device> device;
	try
	{
		mngl::runtime::ensure();
		device.emplace();
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "No usable SDL GPU backend on this runner: " << error.what() << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Unexpected GPU initialization failure: " << error.what() << '\n';
		return 1;
	}

	try
	{
		if (!device->raw())
		{
			std::cerr << "SDL GPU device creation returned an empty handle.\n";
			return 77;
		}
		const char* driver = SDL_GetGPUDeviceDriver(device->raw());
		if (!driver)
		{
			std::cerr << "SDL GPU device did not report its driver.\n";
			return 1;
		}
		const auto svg_format = mngl::two_d::detail::svg::select_svg_shader_format(*device);
		const auto supported = device->shader_format();
		if ((supported & static_cast<SDL_GPUShaderFormat>(svg_format)) == 0)
		{
			std::cerr << "SVG shader format selection returned a format unsupported by the active device.\n";
			return 1;
		}
		std::cout << "SDL GPU driver: " << driver << " (SVG shader format "
		          << static_cast<unsigned int>(svg_format) << ")\n";
		frame_graph_order_cache_invalidation(*device);
		cached_layer_fragmentation_is_bounded();
		test_partial_buffer_updates(*device);
		test_texture_usage(*device);
		test_render_pass_uniforms(*device);
		test_texture_update_preserves_pending_reads(*device);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Unexpected GPU runtime smoke failure: " << error.what() << '\n';
		return 1;
	}
}
