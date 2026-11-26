#define APP_NAME "MNGL GPU runtime smoke"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/device.hpp>
#include <MNGL/detail/two_d/svg_effect_renderer.hpp>
#include <MNGL/frame_graph.hpp>
#include <MNGL/renderer2d.hpp>

#include <exception>
#include <iostream>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
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
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		const mngl::gpu::device device;
		if (!device.raw())
		{
			std::cerr << "SDL GPU device creation returned an empty handle.\n";
			return 77;
		}
		const char* driver = SDL_GetGPUDeviceDriver(device.raw());
		if (!driver)
		{
			std::cerr << "SDL GPU device did not report its driver.\n";
			return 1;
		}
		const auto svg_format = mngl::two_d::detail::svg::select_svg_shader_format(device);
		const auto supported = device.shader_format();
		if ((supported & static_cast<SDL_GPUShaderFormat>(svg_format)) == 0)
		{
			std::cerr << "SVG shader format selection returned a format unsupported by the active device.\n";
			return 1;
		}
		std::cout << "SDL GPU driver: " << driver << " (SVG shader format "
		          << static_cast<unsigned int>(svg_format) << ")\n";
		frame_graph_order_cache_invalidation(device);
		cached_layer_fragmentation_is_bounded();
		return 0;
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "No usable SDL GPU backend on this runner: " << error.what() << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Unexpected GPU runtime smoke failure: " << error.what() << '\n';
		return 1;
	}
}
