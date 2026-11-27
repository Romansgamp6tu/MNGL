#define APP_NAME "MNGL SVG offscreen runtime test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/detail/two_d/svg_effect_renderer.hpp>
#include <MNGL/renderer2d.hpp>
#include <MNGL/svg_object.hpp>
#include <MNGL/svg_sprite.hpp>
#include <MNGL/texture.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
	constexpr glm::uvec2 target_size{64, 64};
	constexpr std::size_t bytes_per_pixel = 4;

	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void temporary_target_budget_rejects_overflow()
	{
		struct fake_target
		{
			glm::uvec2 size{};
			[[nodiscard]] glm::uvec2 get_size() const noexcept { return size; }
		};
		auto budget = std::make_shared<mngl::two_d::detail::svg::temporary_target_budget>();
		budget->byte_limit = 16u * 16u * 8u;
		budget->target_limit = 1;
		mngl::two_d::detail::svg::temporary_target_pool<fake_target> output_targets(budget);
		mngl::two_d::detail::svg::temporary_target_pool<fake_target> filter_targets(budget);
		std::size_t allocations = 0;
		const auto create = [&](glm::uvec2 size)
		{
			++allocations;
			return std::make_shared<fake_target>(fake_target{size});
		};

		auto active_output = output_targets.acquire({16, 16}, create);
		bool rejected = false;
		try
		{
			(void)filter_targets.acquire({8, 8}, create);
		}
		catch (const std::length_error&)
		{
			rejected = true;
		}
		require(rejected && allocations == 1 && budget->cached_bytes == 16u * 16u * 8u,
		        "An active target must make the shared budget reject overflow before another allocation");

		active_output.reset();
		auto smaller_target = filter_targets.acquire({8, 8}, create);
		require(allocations == 2 && output_targets.size() == 0 && filter_targets.size() == 1 &&
		            budget->cached_bytes == 8u * 8u * 8u,
		        "An idle target from another effect stage must be evicted before a smaller allocation");
	}

	void temporary_targets_reuse_with_command_ownership()
	{
		struct fake_target
		{
			glm::uvec2 size{};
			[[nodiscard]] glm::uvec2 get_size() const noexcept { return size; }
		};
		auto baseline_budget = std::make_shared<mngl::two_d::detail::svg::temporary_target_budget>();
		baseline_budget->byte_limit = 12u * 8u * 8u * 8u;
		baseline_budget->target_limit = 12;
		mngl::two_d::detail::svg::temporary_target_pool<fake_target> baseline_pool(baseline_budget);
		std::vector<std::shared_ptr<fake_target>> baseline_command_owned;
		std::size_t baseline_allocations = 0;
		for (int pass = 0; pass < 12; ++pass)
		{
			auto target = baseline_pool.acquire({8, 8}, [&](glm::uvec2 size)
			                                    {
				                                    ++baseline_allocations;
				                                    return std::make_shared<fake_target>(fake_target{size});
			                                    });
			baseline_command_owned.push_back(std::move(target));
		}
		require(baseline_allocations == 12,
		        "Before command-aware reuse, twelve command-retained passes allocated twelve same-size targets");

		auto budget = std::make_shared<mngl::two_d::detail::svg::temporary_target_budget>();
		budget->byte_limit = 2u * 8u * 8u * 8u;
		budget->target_limit = 2;
		mngl::two_d::detail::svg::temporary_target_pool<fake_target> pool(budget);
		auto command_state = std::make_shared<mngl::two_d::detail::svg::temporary_target_command_state>();
		std::vector<std::shared_ptr<fake_target>> command_owned;
		std::size_t allocations = 0;
		const auto create = [&](glm::uvec2 size)
		{
			++allocations;
			return std::make_shared<fake_target>(fake_target{size});
		};
		const auto retain_once = [&](const std::shared_ptr<fake_target>& target)
		{
			if (command_state->retained_targets.insert(target.get()).second)
				command_owned.push_back(target);
			pool.mark_command_owned(target, command_state);
		};

		auto current = pool.acquire({8, 8}, create, command_state);
		retain_once(current);
		for (int pass = 1; pass < 12; ++pass)
		{
			auto next = pool.acquire({8, 8}, create, command_state);
			retain_once(next);
			current = std::move(next);
		}
		require(allocations == 2 && pool.size() == 2 && command_owned.size() == 2,
		        "Repeated same-size passes must reuse two targets while retaining each raw target once per command");

		auto other_command = std::make_shared<mngl::two_d::detail::svg::temporary_target_command_state>();
		bool rejected = false;
		try
		{
			(void)pool.acquire({4, 4}, create, other_command);
		}
		catch (const std::length_error&)
		{
			rejected = true;
		}
		require(rejected && allocations == 2,
		        "Command-owned targets must remain pinned against eviction until command completion");

		std::weak_ptr<mngl::two_d::detail::svg::temporary_target_command_state> in_flight = command_state;
		current.reset();
		command_owned.clear();
		command_state.reset();
		require(in_flight.expired(), "Completing or canceling a command must release its target-pinning state");
		auto after_completion = pool.acquire({8, 8}, create);
		require(allocations == 2 && pool.size() == 2,
		        "Completed command targets must become reusable without another GPU allocation");

		auto external_budget = std::make_shared<mngl::two_d::detail::svg::temporary_target_budget>();
		external_budget->byte_limit = 8u * 8u * 8u;
		external_budget->target_limit = 1;
		mngl::two_d::detail::svg::temporary_target_pool<fake_target> external_pool(external_budget);
		auto external_state = std::make_shared<mngl::two_d::detail::svg::temporary_target_command_state>();
		std::vector<std::shared_ptr<fake_target>> external_command_owned;
		std::size_t external_allocations = 0;
		const auto create_external = [&](glm::uvec2 size)
		{
			++external_allocations;
			return std::make_shared<fake_target>(fake_target{size});
		};
		for (int frame = 0; frame < 40; ++frame)
		{
			auto capture = external_pool.acquire({8, 8}, create_external, external_state);
			if (external_state->retained_targets.insert(capture.get()).second)
				external_command_owned.push_back(capture);
			external_pool.mark_command_owned(capture, external_state);
		}
		require(external_allocations == 1 && external_command_owned.size() == 1,
		        "Repeated external-command frames must reuse retained capture targets within the shared budget");
	}

	std::filesystem::path fixture(std::string_view name)
	{
		return std::filesystem::path(MNGL_SVG_TEST_ASSET_DIRECTORY) / name;
	}

	struct readback_buffer
	{
		SDL_GPUDevice* device = nullptr;
		SDL_GPUTransferBuffer* buffer = nullptr;
		~readback_buffer()
		{
			if (buffer)
				SDL_ReleaseGPUTransferBuffer(device, buffer);
		}
	};

	std::vector<std::uint8_t> read_rgba8(const mngl::gpu::device& device, const mngl::texture& target)
	{
		const std::size_t byte_count = static_cast<std::size_t>(target.width()) * target.height() * bytes_per_pixel;
		if (byte_count > std::numeric_limits<Uint32>::max())
			throw std::length_error("SVG runtime target exceeds SDL's transfer-buffer size limit");

		SDL_GPUTransferBufferCreateInfo info{};
		info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		info.size = static_cast<Uint32>(byte_count);
		readback_buffer transfer{device.raw(), SDL_CreateGPUTransferBuffer(device.raw(), &info)};
		if (!transfer.buffer)
			throw mngl::sdl_error("Failed to create SVG runtime readback buffer: ");

		mngl::gpu::command_buffer command(device);
		auto* copy = SDL_BeginGPUCopyPass(command.raw());
		if (!copy)
			throw mngl::sdl_error("Failed to begin SVG runtime readback pass: ");

		SDL_GPUTextureRegion source{};
		source.texture = target.raw_gpu_texture().raw();
		source.w = target.width();
		source.h = target.height();
		source.d = 1;
		SDL_GPUTextureTransferInfo destination{};
		destination.transfer_buffer = transfer.buffer;
		destination.pixels_per_row = target.width();
		destination.rows_per_layer = target.height();
		SDL_DownloadFromGPUTexture(copy, &source, &destination);
		SDL_EndGPUCopyPass(copy);
		command.submit();
		(void)device.wait();

		void* mapped = SDL_MapGPUTransferBuffer(device.raw(), transfer.buffer, false);
		if (!mapped)
			throw mngl::sdl_error("Failed to map SVG runtime readback buffer: ");
		std::vector<std::uint8_t> pixels(byte_count);
		std::memcpy(pixels.data(), mapped, byte_count);
		SDL_UnmapGPUTransferBuffer(device.raw(), transfer.buffer);
		return pixels;
	}

	using rgba8 = std::array<std::uint8_t, 4>;

	rgba8 pixel_at_row(const std::vector<std::uint8_t>& pixels, std::uint32_t x, std::uint32_t row)
	{
		const auto offset = (static_cast<std::size_t>(row) * target_size.x + x) * bytes_per_pixel;
		return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
	}

	rgba8 pixel_at_world(const std::vector<std::uint8_t>& pixels, std::uint32_t x, std::uint32_t y,
	                     bool flipped)
	{
		return pixel_at_row(pixels, x, flipped ? target_size.y - 1 - y : y);
	}

	bool channel_near(std::uint8_t value, int expected, int tolerance = 3)
	{
		return std::abs(static_cast<int>(value) - expected) <= tolerance;
	}

	bool is_red(const rgba8& pixel)
	{
		return pixel[0] > 240 && pixel[1] < 16 && pixel[2] < 32 && pixel[3] > 240;
	}

	bool is_red_gradient_sample(const rgba8& pixel)
	{
		return pixel[0] > 205 && pixel[1] < 24 && pixel[2] < 60 && pixel[3] > 240;
	}

	bool is_opaque_blue(const rgba8& pixel)
	{
		return pixel[2] > 180 && pixel[0] < 60 && pixel[1] < 60 && pixel[3] > 180;
	}

	bool row_is_flipped(const std::vector<std::uint8_t>& pixels)
	{
		const auto direct = pixel_at_world(pixels, 6, 8, false);
		const auto flipped = pixel_at_world(pixels, 6, 8, true);
		if (is_red(direct) || is_red_gradient_sample(direct))
			return false;
		if (is_red(flipped) || is_red_gradient_sample(flipped))
			return true;
		const auto blue_direct = pixel_at_world(pixels, 16, 16, false);
		const auto blue_flipped = pixel_at_world(pixels, 16, 16, true);
		if (is_opaque_blue(blue_direct))
			return false;
		if (is_opaque_blue(blue_flipped))
			return true;
		throw std::runtime_error(std::format(
		    "Offscreen SVG output did not contain a known orientation anchor "
		    "(red direct={},{},{},{}; red flipped={},{},{},{}; blue direct={},{},{},{}; blue flipped={},{},{},{})",
		    direct[0], direct[1], direct[2], direct[3], flipped[0], flipped[1], flipped[2], flipped[3],
		    blue_direct[0], blue_direct[1], blue_direct[2], blue_direct[3], blue_flipped[0], blue_flipped[1],
		    blue_flipped[2], blue_flipped[3]));
	}

	bool row_is_flipped_from_red_anchor(const std::vector<std::uint8_t>& pixels, std::uint32_t x,
	                                    std::uint32_t y)
	{
		const auto direct = pixel_at_world(pixels, x, y, false);
		const auto flipped = pixel_at_world(pixels, x, y, true);
		if (is_red(direct))
			return false;
		if (is_red(flipped))
			return true;
		throw std::runtime_error(std::format(
		    "Filtered SVG output has no opaque-red orientation anchor at ({},{}): "
		    "direct={},{},{},{} flipped={},{},{},{}",
		    x, y, direct[0], direct[1], direct[2], direct[3], flipped[0], flipped[1], flipped[2], flipped[3]));
	}

	std::optional<mngl::svg_hit_test_element> hit_element_at_world(const mngl::texture& target,
	                                                              glm::vec2 position)
	{
		const auto result = target.hit_test({position.x, static_cast<float>(target_size.y) - position.y});
		if (!result || result->path.empty() || !result->path.back().svg_element)
			return std::nullopt;
		return result->path.back().svg_element;
	}

	std::optional<std::string> hit_id_at_world(const mngl::texture& target, glm::vec2 position)
	{
		const auto element = hit_element_at_world(target, position);
		if (!element)
			return std::nullopt;
		return element->id;
	}

	std::vector<std::uint8_t> render(const mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                 mngl::texture& target,
	                                 const mngl::drawable& drawable, const mngl::view& view)
	{
		auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		frame.set_view(view);
		frame.draw(drawable);
		frame.finish();
		return read_rgba8(device, target);
	}

	void compare_pixels(const std::vector<std::uint8_t>& cpu, const std::vector<std::uint8_t>& sprite)
	{
		require(cpu.size() == sprite.size(), "CPU and sprite SVG readbacks must have equal sizes");
		for (std::size_t index = 0; index < cpu.size(); ++index)
		{
			if (std::abs(static_cast<int>(cpu[index]) - static_cast<int>(sprite[index])) > 2)
				throw std::runtime_error("CPU-submitted SVG and svg_sprite differ in offscreen pixels");
		}
	}

	struct rendered_document
	{
		std::vector<std::uint8_t> cpu_pixels;
		std::vector<std::uint8_t> sprite_pixels;
		bool flipped = false;
	};

	rendered_document render_document(const mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                  mngl::texture& target, const std::filesystem::path& path)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		auto cpu_object = mngl::two_d::svg_object::from_file(path, options);
		auto gpu_object = mngl::two_d::svg_object::from_file(path, options);
		gpu_object.upload(device);
		mngl::two_d::svg_sprite sprite(gpu_object);

		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		rendered_document result;
		result.cpu_pixels = render(device, renderer, target, cpu_object, view);
		result.flipped = row_is_flipped(result.cpu_pixels);
		result.sprite_pixels = render(device, renderer, target, sprite, view);
		compare_pixels(result.cpu_pixels, result.sprite_pixels);
		return result;
	}

	void solid_and_marker_clip(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                           mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target, fixture("svg_runtime_basic.svg"));
		const auto solid = pixel_at_world(rendered.cpu_pixels, 8, 8, rendered.flipped);
		require(is_red(solid), "Solid SVG fixture must render the expected red pixel");
		const auto clear = pixel_at_world(rendered.cpu_pixels, 60, 60, rendered.flipped);
		require(clear[3] < 4, "Pixels outside the SVG must remain transparent");
		const auto solid_hit = hit_id_at_world(target, {8.5f, 8.5f});
		require(solid_hit == "solid",
		        solid_hit ? ("Hit testing returned unexpected solid SVG ID: " + *solid_hit).c_str()
		                  : "Hit testing returned no solid SVG element ID");
		require(hit_id_at_world(target, {60.5f, 60.5f}) == std::nullopt,
		        "Hit testing must return no element outside SVG geometry");

		const auto marker_pixel = pixel_at_world(rendered.cpu_pixels, 42, 32, rendered.flipped);
		require(marker_pixel[1] > 240 && marker_pixel[0] < 16 && marker_pixel[2] < 16,
		        "The marker viewport must clip its oversized green path to the visible region");
		require(hit_id_at_world(target, {42.5f, 32.5f}) == "marker-line",
		        "A visible clipped marker hit must keep its owner element identity");
		require(hit_id_at_world(target, {45.5f, 32.5f}) == std::nullopt,
		        "The marker hit region must not extend beyond its clipped viewport");
	}

	void marker_image_hit_clipping(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                               mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		auto object = mngl::two_d::svg_object::from_file(
		    fixture("svg_runtime_marker_image_clip.svg"), options);
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		(void)render(device, renderer, target, object, view);
		require(hit_id_at_world(target, {31.5f, 11.5f}) == "image-owner",
		        "A visible marker image hit must retain the originating path ID");
		require(hit_id_at_world(target, {28.5f, 11.5f}) == std::nullopt &&
		            hit_id_at_world(target, {35.5f, 11.5f}) == std::nullopt,
		        "Marker image hits must stop at both horizontal viewport edges");
		require(hit_id_at_world(target, {31.5f, 31.5f}) == "effect-owner",
		        "A visible marker image inside an effect layer must retain the originating path ID");
		require(hit_id_at_world(target, {28.5f, 31.5f}) == std::nullopt &&
		            hit_id_at_world(target, {35.5f, 31.5f}) == std::nullopt,
		        "Marker image effect hits must stop at both horizontal viewport edges");
	}

	void gradient_and_group_opacity(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target, fixture("svg_runtime_materials.svg"));
		const auto gradient_left = pixel_at_world(rendered.cpu_pixels, 6, 8, rendered.flipped);
		const auto gradient_right = pixel_at_world(rendered.cpu_pixels, 26, 8, rendered.flipped);
		require(gradient_left[0] > 210 && gradient_left[2] < 60 && gradient_left[3] > 240,
		        "The left gradient sample must remain red and opaque");
		require(gradient_right[2] > 210 && gradient_right[0] < 60 && gradient_right[3] > 240,
		        "The right gradient sample must remain blue and opaque");

		std::vector<std::string> failures;
		const auto overlap = pixel_at_world(rendered.cpu_pixels, 16, 40, rendered.flipped);
		if (!(channel_near(overlap[2], overlap[3], 3) && overlap[0] < 8 && overlap[1] < 8 &&
		      channel_near(overlap[3], 128, 3)))
			failures.push_back(std::format(
			    "Issue #54 group overlap expected premultiplied blue alpha 128, received rgba({}, {}, {}, {})",
			    overlap[0], overlap[1], overlap[2], overlap[3]));
		if (hit_id_at_world(target, {16.5f, 40.5f}) != "blue")
			failures.emplace_back("Overlapping SVG hit testing must return the topmost child ID");
		const auto element_fill = pixel_at_world(rendered.cpu_pixels, 44, 40, rendered.flipped);
		const auto element_stroke = pixel_at_world(rendered.cpu_pixels, 36, 40, rendered.flipped);
		if (!(channel_near(element_fill[0], element_fill[3], 3) && element_fill[1] < 8 &&
		      element_fill[2] < 8 && channel_near(element_fill[3], 128, 3)))
			failures.push_back(std::format("Issue #54 element fill expected red alpha 128, received rgba({}, {}, {}, {})",
			                               element_fill[0], element_fill[1], element_fill[2], element_fill[3]));
		if (!(channel_near(element_stroke[2], element_stroke[3], 3) && element_stroke[0] < 8 &&
		      element_stroke[1] < 8 && channel_near(element_stroke[3], 128, 3)))
			failures.push_back(std::format("Issue #54 element stroke expected blue alpha 128, received rgba({}, {}, {}, {})",
			                               element_stroke[0], element_stroke[1], element_stroke[2], element_stroke[3]));
		if (!failures.empty())
			throw std::runtime_error(failures.front());
	}

	void degenerate_gradients_use_last_stop(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                        mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_degenerate_gradients.svg"));
		constexpr std::array<std::uint32_t, 3> sample_x{12, 32, 52};
		for (const auto x : sample_x)
		{
			require(is_opaque_blue(pixel_at_world(rendered.cpu_pixels, x, 50, rendered.flipped)),
			        "Coincident linear gradients must paint their last stop for pad, repeat, and reflect");
			require(is_opaque_blue(pixel_at_world(rendered.cpu_pixels, x, 20, rendered.flipped)),
			        "Zero-radius radial gradients must paint their last stop for pad, repeat, and reflect");
		}
	}

	bool is_opaque_gradient_midpoint(const rgba8& pixel)
	{
		return pixel[0] > 100 && pixel[0] < 155 && pixel[1] < 24 && pixel[2] > 100 && pixel[2] < 155 &&
		       pixel[3] > 240;
	}

	void small_nonzero_gradients_remain_nondegenerate(mngl::gpu::device& device,
	                                                  mngl::two_d::renderer& renderer,
	                                                  mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_small_nonzero_gradients.svg"));
		const auto linear = pixel_at_world(rendered.cpu_pixels, 16, 32, rendered.flipped);
		const auto radial = pixel_at_world(rendered.cpu_pixels, 48, 32, rendered.flipped);
		require(is_opaque_gradient_midpoint(linear),
		        "A tiny nonzero linear gradient must interpolate instead of collapsing to its last stop");
		require(is_opaque_gradient_midpoint(radial),
		        "A tiny nonzero radial radius must interpolate instead of collapsing to its last stop");
	}

	void equal_gradient_offsets_choose_later_stop(mngl::gpu::device& device,
	                                              mngl::two_d::renderer& renderer, mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_equal_offset_gradient.svg"));
		const auto repeated_zero = pixel_at_world(rendered.cpu_pixels, 16, 44, rendered.flipped);
		const auto before_transition = pixel_at_world(rendered.cpu_pixels, 31, 16, rendered.flipped);
		const auto hard_transition = pixel_at_world(rendered.cpu_pixels, 32, 16, rendered.flipped);
		const auto after_transition = pixel_at_world(rendered.cpu_pixels, 33, 16, rendered.flipped);
		require(is_opaque_blue(repeated_zero),
		        "A repeated gradient parameter at offset zero must choose the later stop");
		require(before_transition[2] > 210 && before_transition[0] < 48 && before_transition[1] < 24 &&
		            before_transition[3] > 240,
		        "Immediately before a shared offset, the earlier blue segment must still be sampled");
		require(hard_transition[1] > 220 && hard_transition[0] < 24 && hard_transition[2] < 24 &&
		            hard_transition[3] > 240,
		        "At a shared intermediate offset the later green stop must win");
		require(after_transition[1] > 220 && after_transition[0] > 8 && after_transition[0] < 60 &&
		            after_transition[2] > 8 && after_transition[2] < 60 && after_transition[3] > 240,
		        "Immediately after a shared offset, interpolation must continue from the later green stop");
	}

	void use_instances_hit_test(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                            mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_use_hit_instances.svg"));
		const auto first_left_hit = hit_element_at_world(target, {12.5f, 12.5f});
		const auto second_left_hit = hit_element_at_world(target, {40.5f, 40.5f});
		const auto first_right_hit = hit_element_at_world(target, {24.5f, 12.5f});
		const auto second_right_hit = hit_element_at_world(target, {52.5f, 40.5f});
		require(first_left_hit && first_left_hit->id == "tile-left" &&
		            second_left_hit && second_left_hit->id == "tile-left",
		        "Both placed use instances must hit the referenced left child ID");
		require(first_left_hit->index != second_left_hit->index,
		        "Separate use instances must retain independent hit indices for the same child");
		require(first_right_hit && first_right_hit->id == "tile-right" &&
		            second_right_hit && second_right_hit->id == "tile-right",
		        "Both placed use instances must hit the referenced right child ID");
		require(first_right_hit->index != second_right_hit->index,
		        "Separate use instances must retain independent hit indices for each repeated child");
		const auto first_red = pixel_at_world(rendered.cpu_pixels, 12, 12, rendered.flipped);
		const auto second_red = pixel_at_world(rendered.cpu_pixels, 40, 40, rendered.flipped);
		if (!is_red(first_red) || !is_red(second_red))
			throw std::runtime_error(std::format(
			    "Both use instances must render red children (first rgba={},{},{},{}, second rgba={},{},{},{})",
			    first_red[0], first_red[1], first_red[2], first_red[3], second_red[0], second_red[1], second_red[2],
			    second_red[3]));
		require(is_opaque_blue(pixel_at_world(rendered.cpu_pixels, 24, 12, rendered.flipped)) &&
		            is_opaque_blue(pixel_at_world(rendered.cpu_pixels, 52, 40, rendered.flipped)),
		        "Both use instances must render their blue child at the instance-specific position");
	}

	void inherited_gradient_href_renders_like_explicit_gradient(mngl::gpu::device& device,
	                                                             mngl::two_d::renderer& renderer,
	                                                             mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_gradient_template_inheritance.svg"));
		constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 2> sample_pairs{{{10, 42}, {22, 54}}};
		std::array<rgba8, 2> inherited_samples{};
		for (std::size_t index = 0; index < sample_pairs.size(); ++index)
		{
			const auto [inherited_x, explicit_x] = sample_pairs[index];
			const auto inherited = pixel_at_world(rendered.cpu_pixels, inherited_x, 10, rendered.flipped);
			const auto explicit_gradient = pixel_at_world(rendered.cpu_pixels, explicit_x, 10, rendered.flipped);
			inherited_samples[index] = inherited;
			for (std::size_t channel = 0; channel < inherited.size(); ++channel)
				require(std::abs(static_cast<int>(inherited[channel]) -
				                static_cast<int>(explicit_gradient[channel])) <= 2,
				        "A chained gradient href must render like the equivalent explicit stops and coordinates");
		}
		require(is_red_gradient_sample(inherited_samples[0]),
		        "The inherited gradient's early sample must interpolate from its inherited red stop");
		require(is_opaque_blue(inherited_samples[1]),
		        "The inherited gradient's late sample must interpolate toward its inherited blue stop");
	}

	void empty_gradient_preserves_pixels_and_underlying_hit(mngl::gpu::device& device,
	                                                        mngl::two_d::renderer& renderer,
	                                                        mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_empty_gradient_hit.svg"));
		const auto control = pixel_at_world(rendered.cpu_pixels, 44, 16, rendered.flipped);
		const auto covered = pixel_at_world(rendered.cpu_pixels, 12, 16, rendered.flipped);
		require(is_red(control) && is_red(covered),
		        "An empty gradient overlay must leave the lower red fill unchanged");
		for (std::size_t channel = 0; channel < control.size(); ++channel)
			require(control[channel] == covered[channel],
			        "An empty gradient overlay must match the unobscured control pixels exactly");
		require(hit_id_at_world(target, {12.5f, 16.5f}) == "under",
		        "An empty gradient must not intercept hits from the lower painted element");
		require(hit_id_at_world(target, {44.5f, 16.5f}) == "control",
		        "The unobscured control shape must remain hit-testable");
	}

	void css_pointer_events_none_keeps_paint_and_passes_hits_through(mngl::gpu::device& device,
	                                                                 mngl::two_d::renderer& renderer,
	                                                                 mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_css_pointer_events_none.svg"));
		const auto overlap = pixel_at_world(rendered.cpu_pixels, 24, 24, rendered.flipped);
		const auto under_only = pixel_at_world(rendered.cpu_pixels, 12, 24, rendered.flipped);
		const auto decoration_only = pixel_at_world(rendered.cpu_pixels, 42, 24, rendered.flipped);
		if (!is_opaque_blue(overlap) || !is_opaque_blue(decoration_only))
			throw std::runtime_error(std::format(
			    "CSS pointer-events none must preserve blue paint (overlap rgba={},{},{},{}, outside rgba={},{},{},{})",
			    overlap[0], overlap[1], overlap[2], overlap[3], decoration_only[0], decoration_only[1],
			    decoration_only[2], decoration_only[3]));
		require(is_red(under_only), "The lower element must remain visible outside the decoration");
		require(hit_id_at_world(target, {24.5f, 24.5f}) == "under",
		        "A visible CSS decoration with pointer-events none must pass the overlap hit to the underlay");
		require(hit_id_at_world(target, {12.5f, 24.5f}) == "under",
		        "The underlay must remain hit-testable where no decoration covers it");
		require(hit_id_at_world(target, {42.5f, 24.5f}) == std::nullopt,
		        "A pointer-events none decoration outside the underlay must not produce a hit");
	}

	void adjacent_effect_draws_resolve_independently(mngl::gpu::device& device,
	                                                mngl::two_d::renderer& renderer,
	                                                mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_adjacent_effect_draws.svg"));
		const auto left = pixel_at_world(rendered.cpu_pixels, 12, 12, rendered.flipped);
		const auto right = pixel_at_world(rendered.cpu_pixels, 48, 12, rendered.flipped);
		require(is_red(left),
		        "The first deferred SVG effect must resolve to its own red capture texture");
		require(is_opaque_blue(right),
		        "The second deferred SVG effect must resolve to its own blue capture texture");
	}

	void zero_region_svg_effect_is_noop(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                    mngl::texture& target)
	{
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		mngl::two_d::svg_object zero_region(R"svg(
		    <svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
		      <defs><filter id="empty" filterUnits="userSpaceOnUse" x="0" y="0" width="0" height="64">
		        <feFlood flood-color="blue"/>
		      </filter></defs>
	      <rect x="0" y="0" width="40" height="40" fill="red"/>
	      <rect x="0" y="0" width="40" height="40" fill="blue" filter="url(#empty)"/>
		    </svg>)svg");
		const auto zero_region_pixels = render(device, renderer, target, zero_region, view);
		const bool zero_region_flipped = row_is_flipped(zero_region_pixels);
		require(is_red(pixel_at_world(zero_region_pixels, 20, 20, zero_region_flipped)),
		        "A zero-width SVG filter region must emit no deferred draw and preserve the underlay");
	}

	void visible_and_culled_svg_effects_resolve_independently(mngl::gpu::device& device,
	                                                          mngl::two_d::renderer& renderer,
	                                                          mngl::texture& target)
	{
		const auto make_effect = [](std::string_view id, std::string_view fill)
		{
			return mngl::two_d::svg_object(std::format(R"svg(
			    <svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
			      <defs><filter id="soft"><feGaussianBlur stdDeviation="1"/></filter></defs>
			      <rect id="{}" x="4" y="4" width="36" height="36" fill="{}" filter="url(#soft)"/>
			    </svg>)svg",
			                                  id, fill));
		};
		auto visible_object = make_effect("visible", "#ff0000");
		visible_object.upload(device);
		mngl::two_d::svg_sprite visible(visible_object);
		auto culled_object = make_effect("culled", "#0000ff");
		culled_object.upload(device);
		mngl::two_d::svg_sprite culled(culled_object);
		culled.position({128, 128});

		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		frame.set_view(view);
		frame.draw(visible);
		frame.draw(culled);
		frame.finish();
		const auto pixels = read_rgba8(device, target);
		const bool flipped = row_is_flipped_from_red_anchor(pixels, 12, 12);
		require(is_red(pixel_at_world(pixels, 12, 12, flipped)),
		        "A culled deferred effect must not prevent an adjacent visible effect from resolving");
		require(pixel_at_world(pixels, 60, 60, flipped)[3] < 4,
		        "A culled deferred effect must not paint into the visible frame");
	}

	void worker_recording_effects_resolve_independently(mngl::gpu::device& device,
	                                                    mngl::two_d::renderer& renderer,
	                                                    mngl::texture& target)
	{
		const auto make_effect = [](std::string_view id, std::string_view fill, int x)
		{
			return mngl::two_d::svg_object(std::format(R"svg(
			    <svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
			      <defs><filter id="soft-edge" x="-0.25" y="-0.25" width="1.5" height="1.5">
			        <feGaussianBlur stdDeviation="1"/>
			      </filter></defs>
			      <rect id="{}" x="{}" y="4" width="36" height="36" fill="{}"
			            filter="url(#soft-edge)"/>
			    </svg>)svg",
			                                  id, x, fill));
		};
		auto red = make_effect("worker-red", "#ff0000", 4);
		auto blue = make_effect("worker-blue", "#0000ff", 16);
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});

		const auto render_pair = [&](bool order_independent)
		{
			const bool previous_hit_testing = target.hit_testing_enabled();
			if (order_independent && previous_hit_testing)
				target.enable_hit_testing(false);
			struct hit_testing_restore
			{
				mngl::texture& target;
				bool enabled;
				~hit_testing_restore() { target.enable_hit_testing(enabled); }
			} restore{target, previous_hit_testing};
			auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
			frame.set_view(view);
			if (order_independent)
			{
				frame.draw_order_independent([&](mngl::two_d::frame_2d& group)
				                            {
					                            group.draw(red);
					                            group.draw(blue);
				                            });
			}
			else
			{
				auto red_context = frame.create_recording_context();
				auto blue_context = frame.create_recording_context();
				std::exception_ptr red_error;
				std::exception_ptr blue_error;
				std::thread red_worker;
				std::thread blue_worker;
				struct thread_join_guard
				{
					std::thread& first;
					std::thread& second;
					~thread_join_guard()
					{
						if (first.joinable())
							first.join();
						if (second.joinable())
							second.join();
					}
				} join_guard{red_worker, blue_worker};
				red_worker = std::thread([&]
				                        {
				                        try
				                        {
					                        red_context.draw(red);
					                        red_context.finish();
				                        }
				                        catch (...)
				                        {
					                        red_error = std::current_exception();
				                        }
				                        });
				blue_worker = std::thread([&]
				                         {
				                         try
				                         {
					                         blue_context.draw(blue);
					                         blue_context.finish();
					                         }
					                         catch (...)
					                         {
						                         blue_error = std::current_exception();
					                         }
					                         });
				red_worker.join();
				blue_worker.join();
				if (red_error)
					std::rethrow_exception(red_error);
				if (blue_error)
					std::rethrow_exception(blue_error);
				frame.merge(std::move(red_context));
				frame.merge(std::move(blue_context));
			}
			frame.finish();
			return read_rgba8(device, target);
		};

		const auto worker_pixels = render_pair(false);
		bool worker_flipped = false;
		try
		{
			worker_flipped = row_is_flipped_from_red_anchor(worker_pixels, 12, 12);
		}
		catch (const std::exception& error)
		{
			throw std::runtime_error(std::format("Merged worker contexts: {}", error.what()));
		}
		require(is_red(pixel_at_world(worker_pixels, 12, 12, worker_flipped)),
		        "The first worker effect must resolve to its own red capture texture");
		require(is_opaque_blue(pixel_at_world(worker_pixels, 48, 24, worker_flipped)),
		        "The second worker effect must resolve to its own blue capture texture");
		require(is_opaque_blue(pixel_at_world(worker_pixels, 24, 24, worker_flipped)),
		        "Merged worker effect draws must preserve their frame merge order when overlapping");

		const auto order_independent_pixels = render_pair(true);
		bool order_independent_flipped = false;
		try
		{
			order_independent_flipped = row_is_flipped_from_red_anchor(order_independent_pixels, 12, 12);
		}
		catch (const std::exception& error)
		{
			throw std::runtime_error(std::format("Order-independent draws: {}", error.what()));
		}
		require(is_red(pixel_at_world(order_independent_pixels, 12, 12, order_independent_flipped)),
		        "Order-independent SVG effects must retain the first effect's resolved texture");
		require(is_opaque_blue(pixel_at_world(order_independent_pixels, 48, 24, order_independent_flipped)),
		        "Order-independent SVG effects must retain the second effect's resolved texture");
	}

	void repeated_multicolor_sprite_batches(mngl::gpu::device& device,
	                                        mngl::two_d::renderer& renderer, mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		mngl::two_d::svg_object object(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		    <rect id="red" x="4" y="4" width="36" height="36" fill="#ff0000"/>
		    <rect id="blue" x="16" y="16" width="36" height="36" fill="#0000ff"/>
		    </svg>)svg", options);
		object.upload(device);
		mngl::two_d::svg_sprite sprite(object);
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		frame.set_view(view);
		for (int instance = 0; instance < 100; ++instance)
			frame.draw(sprite);
		frame.finish();
		const auto pixels = read_rgba8(device, target);
		const bool flipped = row_is_flipped(pixels);
		require(is_red(pixel_at_world(pixels, 8, 8, flipped)),
		        "Merged multicolor sprite mesh must retain the first paint color");
		require(is_opaque_blue(pixel_at_world(pixels, 24, 24, flipped)),
		        "Merged multicolor sprite mesh must preserve SVG paint order and colors");
		require(hit_id_at_world(target, {24.5f, 24.5f}) == "blue",
		        "Batched multicolor sprite draws must retain topmost SVG hit identity");
		require(renderer.last_frame_stats().draw_calls == 1,
		        "One hundred repeated multicolor SVG sprites must batch into one draw call");
	}

	void gradient_color_interpolation(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                  mngl::texture& target)
	{
		const auto rendered = render_document(device, renderer, target,
		                                      fixture("svg_runtime_color_interpolation.svg"));
		const auto srgb = pixel_at_world(rendered.cpu_pixels, 25, 16, rendered.flipped);
		const auto linear = pixel_at_world(rendered.cpu_pixels, 25, 40, rendered.flipped);
		require(srgb[0] >= 120 && srgb[0] <= 135,
		        "Default SVG gradient interpolation must remain an sRGB midpoint");
		require(linear[0] >= 180 && linear[0] <= 196,
		        "linearRGB SVG gradient interpolation must encode a midpoint near 188/255");
	}

	void filter_primitive_color_interpolation(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                          mngl::texture& target)
	{
		const mngl::two_d::svg_object object(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		    <defs>
		        <filter id="linear-primitive" color-interpolation-filters="sRGB">
		            <feFlood flood-color="#808080" result="gray"/>
		            <feColorMatrix in="gray" type="matrix"
		                values=".5 0 0 0 0 0 .5 0 0 0 0 0 .5 0 0 0 0 0 1 0"
		                color-interpolation-filters="linearRGB"/>
		        </filter>
		        <filter id="srgb-primitive" color-interpolation-filters="linearRGB">
		            <feFlood flood-color="#808080" result="gray"
		                color-interpolation-filters="sRGB"/>
		            <feColorMatrix in="gray" type="matrix"
		                values=".5 0 0 0 0 0 .5 0 0 0 0 0 .5 0 0 0 0 0 1 0"
		                color-interpolation-filters="sRGB"/>
		        </filter>
		    </defs>
		    <rect x="8" y="8" width="16" height="16" fill="white" filter="url(#linear-primitive)"/>
		    <rect x="40" y="8" width="16" height="16" fill="white" filter="url(#srgb-primitive)"/>
		    <rect x="4" y="6" width="4" height="4" fill="red"/>
		</svg>)svg");
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		const auto pixels = render(device, renderer, target, object, view);
		const bool flipped = row_is_flipped(pixels);
		const auto linear = pixel_at_world(pixels, 16, 16, flipped);
		const auto srgb = pixel_at_world(pixels, 48, 16, flipped);
		require(linear[0] >= 78 && linear[0] <= 108,
		        "A linearRGB primitive must convert its sRGB input before applying the color matrix");
		require(srgb[0] >= 52 && srgb[0] <= 78,
		        "An sRGB primitive must preserve encoded input values during the color matrix");
		require(linear[0] > srgb[0] + 20,
		        "Per-primitive color interpolation overrides must affect rendered filter pixels");
	}

	void filter_dependency_pruning_preserves_pixels(mngl::gpu::device& device,
	                                                 mngl::two_d::renderer& renderer,
	                                                 mngl::texture& target)
	{
		constexpr std::string_view source = R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		    <defs><filter id="pruned" x="0" y="0" width="64" height="64"
		        filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse"
		        color-interpolation-filters="linearRGB">
		      <feGaussianBlur in="SourceGraphic" stdDeviation="8" result="unused-blur"/>
		      <feFlood flood-color="#00ff00" result="green" color-interpolation-filters="sRGB"/>
		      <feComposite in="green" in2="SourceGraphic" operator="in"
		          color-interpolation-filters="sRGB"/>
		    </filter></defs>
		    <rect x="2" y="2" width="12" height="12" fill="red"/>
		    <rect x="16" y="16" width="16" height="16" fill="red" filter="url(#pruned)"/>
		    </svg>)svg";
		const auto parsed = mngl::two_d::detail::svg::parse(source);
		require(parsed.geometry.size() == 2 && parsed.geometry[1].effect && parsed.geometry[1].effect->filter,
		        "The filter liveness regression must parse into its expected effect layer");
		const auto& steps = parsed.geometry[1].effect->filter->steps;
		const auto dependencies = mngl::two_d::detail::svg::analyze_filter_dependencies(steps);
		require(steps.size() == 3 && !dependencies.live_outputs[2] && dependencies.live_outputs[3] &&
		            dependencies.live_outputs[4] &&
		            std::count(dependencies.live_outputs.begin() + 2, dependencies.live_outputs.end(), true) == 2,
		        "The unused blur must not produce a filter pass; flood and its join remain live");
		require(steps[1].operation == mngl::two_d::detail::svg::filter_op::flood && steps[1].input == 0 &&
		            dependencies.consumers[2] == 0,
		        "An sRGB flood must not consume or color-convert the preceding linearRGB output");

		const mngl::two_d::svg_object object(source);
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		const auto pixels = render(device, renderer, target, object, view);
		const bool flipped = row_is_flipped(pixels);
		const auto center = pixel_at_world(pixels, 24, 24, flipped);
		require(center[1] > 240 && center[0] < 16 && center[2] < 16 && center[3] > 240,
		        "The live sRGB flood/composite must preserve its green pixels after pruning the blur");
		require(pixel_at_world(pixels, 40, 40, flipped)[3] < 4,
		        "The live composite must still restrict flood output to the source graphic alpha");
	}

	void sync_view_preserves_loaded_media_and_layout(mngl::gpu::device& device,
	                                                 mngl::two_d::renderer& renderer,
	                                                 mngl::texture& target)
	{
		constexpr std::string_view source = R"svg(<svg width="100%" height="100%" viewBox="0 0 100 100">
		    <style>
		      @media (min-width: 500px) { #shape { fill: #ff0000; } }
		      @media (max-width: 499px) { #shape { fill: #0000ff; } }
		    </style>
		    <path id="shape" d="M5 50 C5 5 95 5 95 50 C95 95 5 95 5 50 Z"/>
		    </svg>)svg";
		mngl::two_d::svg_object object(source);
		const auto loaded_size = object.size();
		require(loaded_size == glm::vec2(300, 150),
		        "A percentage root must resolve against the default load viewport");
		const mngl::view view(mngl::rectangle{glm::vec2(0), loaded_size});
		const auto initial_pixels = render(device, renderer, target, object, view);
		const auto initial_center = pixel_at_world(initial_pixels, 32, 32, false);
		require(is_opaque_blue(initial_center), "The initially selected media style must reflect the 300px viewport");

		object.sync_view(view, {800, 600}, 0.25f);
		require(object.size() == loaded_size,
		        "sync_view must preserve the percentage-resolved dimensions from document load");
		const auto synchronized_pixels = render(device, renderer, target, object, view);
		const auto synchronized_center = pixel_at_world(synchronized_pixels, 32, 32, false);
		require(is_opaque_blue(synchronized_center),
		        "Increasing the tessellation viewport must not re-evaluate media queries or change SVG layout");
	}

	void filter_primitive_default_region_follows_input(mngl::gpu::device& device,
	                                                   mngl::two_d::renderer& renderer,
	                                                   mngl::texture& target)
	{
		const mngl::two_d::svg_object object(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
		    <defs><filter id="regions" filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse"
		        x="0" y="0" width="64" height="64">
		        <feFlood flood-color="blue" x="8" y="8" width="12" height="12" result="small"/>
		        <feColorMatrix in="small" result="matrix"
		            values="0 0 0 0 1  0 0 0 0 0  0 0 0 0 0  0 0 0 0 1"/>
		        <feComposite in="matrix" in2="SourceGraphic" operator="in" result="source-limited"/>
		        <feComposite in="source-limited" in2="matrix" operator="over"/>
		    </filter></defs>
		    <rect width="32" height="32" fill="white" filter="url(#regions)"/>
		</svg>)svg");
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		const auto pixels = render(device, renderer, target, object, view);
		std::size_t opaque_red_pixels = 0;
		for (std::size_t offset = 0; offset + 3 < pixels.size(); offset += 4)
		{
			if (pixels[offset] > 240 && pixels[offset + 1] < 16 && pixels[offset + 2] < 32 && pixels[offset + 3] > 240)
				++opaque_red_pixels;
		}
		if (opaque_red_pixels != 144)
		{
			throw std::runtime_error(std::format(
			    "The matrix output must inherit the flood's 12x12 region; received {} red pixels", opaque_red_pixels));
		}
	}

	std::vector<std::uint8_t> render_with_view_change(const mngl::gpu::device& device,
	                                                 mngl::two_d::renderer& renderer,
	                                                 mngl::texture& target,
	                                                 const mngl::drawable& drawable,
	                                                 const mngl::view& initial,
	                                                 const mngl::view& final)
	{
		auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		frame.set_view(initial);
		frame.draw(drawable);
		frame.set_view(final);
		frame.finish();
		return read_rgba8(device, target);
	}

	void filtered_view_changes(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                           mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		auto object = mngl::two_d::svg_object::from_file(fixture("svg_runtime_filtered.svg"), options);
		auto resident_object = mngl::two_d::svg_object::from_file(fixture("svg_runtime_filtered.svg"), options);
		resident_object.upload(device);
		mngl::two_d::svg_sprite sprite(resident_object);

		const mngl::view visible(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		const mngl::view outside(mngl::rectangle{glm::vec2(1000), glm::vec2(target_size)});
		const auto moved_onscreen = render_with_view_change(device, renderer, target, object, outside, visible);
		const auto moved_onscreen_flipped = row_is_flipped(moved_onscreen);
		const auto visible_filtered_pixel = pixel_at_world(moved_onscreen, 16, 16, moved_onscreen_flipped);
		const auto moved_offscreen = render_with_view_change(device, renderer, target, sprite, visible, outside);
		bool transparent = true;
		for (std::size_t offset = 3; offset < moved_offscreen.size(); offset += bytes_per_pixel)
			transparent = transparent && moved_offscreen[offset] < 4;
		if (!(visible_filtered_pixel[2] > 180 && visible_filtered_pixel[3] > 120) || !transparent)
			throw std::runtime_error(std::format(
			    "Issue #86 view transition: outside→visible pixel rgba({}, {}, {}, {}); "
			    "visible→outside alpha is {}",
			    visible_filtered_pixel[0], visible_filtered_pixel[1], visible_filtered_pixel[2],
			    visible_filtered_pixel[3], transparent ? "transparent" : "non-transparent"));
	}

	void external_command_reuses_svg_targets(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                         mngl::texture& target)
	{
		mngl::two_d::svg_object object = mngl::two_d::svg_object::from_file(fixture("svg_runtime_filtered.svg"));
		mngl::two_d::svg_object final_object(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"
		    viewBox="0 0 64 64"><defs><filter id="soft-edge" x="-0.25" y="-0.25" width="1.5" height="1.5">
		      <feGaussianBlur stdDeviation="1"/></filter></defs>
		    <rect id="orientation-anchor" x="4" y="4" width="4" height="8" fill="#ff0000"/>
		    <rect id="final-frame" x="32" y="8" width="16" height="16" fill="#00ff00"
		          filter="url(#soft-edge)"/></svg>)svg");
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		mngl::gpu::command_buffer command(device);
		for (int frame_number = 0; frame_number < 39; ++frame_number)
		{
			auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
			frame.set_view(view);
			frame.draw(object);
			frame.finish(command);
		}
		auto final_frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		final_frame.set_view(view);
		final_frame.draw(final_object);
		final_frame.finish(command);
		command.submit();
		const auto pixels = read_rgba8(device, target);
		const bool flipped = row_is_flipped(pixels);
		require(is_red(pixel_at_world(pixels, 6, 8, flipped)),
		        "Repeated finish_frame calls on one external command must preserve the final SVG output");
		const auto final_center = pixel_at_world(pixels, 40, 16, flipped);
		require(final_center[1] > 180 && final_center[0] < 32 && final_center[2] < 32 && final_center[3] > 120,
		        "The final external-command frame must replace the earlier blue filtered frame with green output");
		const auto blur_edge = pixel_at_world(pixels, 31, 16, flipped);
		require(blur_edge[1] > 8 && blur_edge[3] > 8 && blur_edge[0] < 8 && blur_edge[2] < 8,
		        "The final filtered green shape must blur beyond its unfiltered left edge");
	}

	void clipped_path_hit_test(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                           mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		auto cpu = mngl::two_d::svg_object::from_file(fixture("svg_runtime_clip_path.svg"), options);
			auto gpu = mngl::two_d::svg_object::from_file(fixture("svg_runtime_clip_path.svg"), options);
			gpu.upload(device);
			mngl::two_d::svg_sprite sprite(gpu);
			const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
			auto pixels = render(device, renderer, target, cpu, view);
			const bool flipped = row_is_flipped(pixels);
			require(hit_id_at_world(target, {44.5f, 44.5f}) == "clipped",
			        "A point inside clipPath must hit the clipped element");
			require(hit_id_at_world(target, {34.5f, 44.5f}) == "under",
			        "A point outside clipPath must hit the visible geometry underneath");
			require(pixel_at_world(pixels, 34, 44, flipped)[0] > 240 &&
			            pixel_at_world(pixels, 34, 44, flipped)[2] < 16,
			        "Pixels outside clipPath must show the underlying red shape");
		pixels = render(device, renderer, target, sprite, view);
		require(hit_id_at_world(target, {34.5f, 44.5f}) == "under",
		        "GPU sprite hit testing must obey clipPath boundaries");
	}

	void nested_clip_path_child_union_hit_test(mngl::gpu::device& device,
	                                           mngl::two_d::renderer& renderer,
	                                           mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		mngl::two_d::svg_object object(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"
		    viewBox="0 0 64 64">
		  <defs>
		    <clipPath id="left-local" clipPathUnits="userSpaceOnUse">
		      <rect x="6" y="8" width="8" height="8" transform="translate(2 0)"/>
		    </clipPath>
		    <clipPath id="right-local" clipPathUnits="userSpaceOnUse">
		      <rect x="46" y="12" width="8" height="8" transform="translate(2 0)"/>
		    </clipPath>
		    <clipPath id="disjoint-local" clipPathUnits="userSpaceOnUse">
		      <rect x="48" y="48" width="8" height="8"/>
		    </clipPath>
		    <clipPath id="outer" clipPathUnits="userSpaceOnUse">
		      <rect x="4" y="4" width="20" height="20" clip-path="url(#left-local)"/>
		      <rect x="40" y="4" width="20" height="20" clip-path="url(#right-local)"/>
		      <rect x="24" y="40" width="8" height="8" clip-path="url(#disjoint-local)"/>
		      <path clip-rule="evenodd" d="M20 32H44V56H20Z M28 40H36V48H28Z"/>
		    </clipPath>
		  </defs>
		  <rect id="subject" width="64" height="64" fill="red" clip-path="url(#outer)"/>
		</svg>)svg", options);
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		(void)render(device, renderer, target, object, view);
		require(hit_id_at_world(target, {10.5f, 10.5f}) == "subject",
		        "The first outer clip child must contribute its intersection with its local clipPath");
		require(hit_id_at_world(target, {50.5f, 16.5f}) == "subject",
		        "The second outer clip child must contribute to the clipPath union");
		require(hit_id_at_world(target, {6.5f, 10.5f}) == std::nullopt,
		        "The first outer clip child must not hit outside its local clipPath");
		require(hit_id_at_world(target, {42.5f, 16.5f}) == std::nullopt,
		        "The second outer clip child must not hit outside its local clipPath");
		require(hit_id_at_world(target, {32.5f, 16.5f}) == std::nullopt,
		        "Separate outer clip children must remain a union with an empty gap");
		require(hit_id_at_world(target, {50.5f, 50.5f}) == std::nullopt,
		        "A disjoint local clipPath must contribute no unbounded geometry to its parent union");
		require(hit_id_at_world(target, {24.5f, 36.5f}) == "subject" &&
		            hit_id_at_world(target, {32.5f, 44.5f}) == std::nullopt,
		        "Clip intersections must retain union components and exclude even-odd holes");
	}

	void root_viewport_slice_clip(mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                              mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		auto source = mngl::two_d::svg_object::from_file(fixture("svg_runtime_viewport_slice.svg"), options);
		require(source.size() == glm::vec2(64, 32), "Root SVG size must match its intrinsic viewport");
		source.upload(device);
		mngl::two_d::svg_sprite sprite(source);
		sprite.position({0, 16});
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		const auto pixels = render(device, renderer, target, sprite, view);
		const auto direct_inside = pixel_at_world(pixels, 32, 20, false);
		const auto flipped_inside = pixel_at_world(pixels, 32, 20, true);
		const bool flipped = !is_red(direct_inside) && is_red(flipped_inside);
		require(is_red(flipped ? flipped_inside : direct_inside),
		        "Root viewport clip must preserve pixels inside a slice-mapped SVG");
		for (const auto y : {8u, 55u})
			require(pixel_at_world(pixels, 32, y, flipped)[3] < 4,
			        "Slice overflow must not paint outside the SVG root viewport");
		require(hit_id_at_world(target, {32.5f, 20.5f}) == "overflow",
		        "Hit testing inside the root viewport must retain the SVG element");
		for (const auto y : {8.5f, 55.5f})
			require(hit_id_at_world(target, {32.5f, y}) == std::nullopt,
			        "Hit testing must reject SVG geometry outside the root viewport clip");
	}

	void nested_marker_and_root_slice_hit_clips(mngl::gpu::device& device,
	                                            mngl::two_d::renderer& renderer,
	                                            mngl::texture& target)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		mngl::two_d::svg_object object(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="32"
		    viewBox="0 0 32 32" preserveAspectRatio="xMidYMid slice">
		  <defs><marker id="image-marker" markerUnits="userSpaceOnUse" markerWidth="4" markerHeight="4"
		      viewBox="0 0 4 4" refX="2" refY="2">
		    <image x="-2" y="-2" width="8" height="8"
		           href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4////fwAJ+wP9KobjigAAAABJRU5ErkJggg=="/>
		  </marker></defs>
		  <path id="owner" d="M8 8H16" transform="translate(2 0)" fill="none" stroke="none"
		        marker-end="url(#image-marker)"/>
		</svg>)svg", options);
		object.transform(mngl::two_d::transform(glm::translate(glm::mat4(1.0f), glm::vec3(4, 3, 0))));
		const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
		(void)render(device, renderer, target, object, view);
		require(hit_id_at_world(target, {40.5f, 31.5f}) == "owner",
		        "The intersection of transformed marker and root slice viewports must remain hittable");
		require(hit_id_at_world(target, {34.5f, 31.5f}) == std::nullopt,
		        "A marker image hit outside its transformed marker viewport must be clipped");
		require(hit_id_at_world(target, {40.5f, 36.5f}) == std::nullopt,
		        "A marker image hit outside the transformed root slice viewport must be clipped");
	}

	void gaussian_blur_wrap_interpolates_across_seam(mngl::gpu::device& device,
	                                                 mngl::two_d::renderer& renderer,
	                                                 mngl::texture& target)
	{
		const auto render_edge_mode = [&](std::string_view edge_mode)
		{
			const auto source = std::format(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
			  <defs><filter id="edge" filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse"
			      x="0" y="28" width="8" height="8">
			    <feGaussianBlur stdDeviation="1" edgeMode="{}"/>
			  </filter></defs>
			  <rect x="0" y="28" width="2" height="8" fill="white" filter="url(#edge)"/>
			</svg>)svg", edge_mode);
			mngl::two_d::svg_object object(source);
			const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
			return render(device, renderer, target, object, view);
		};
		const auto none = render_edge_mode("none");
		const auto duplicate = render_edge_mode("duplicate");
		const auto wrap = render_edge_mode("wrap");
		const auto alpha = [](const auto& pixels, std::uint32_t x)
		{
			return std::max(pixel_at_row(pixels, x, 31)[3], pixel_at_row(pixels, x, 32)[3]);
		};
		if (alpha(wrap, 7) <= alpha(none, 7) + 20)
		{
			std::string samples;
			for (std::uint32_t x = 0; x < 8; ++x)
			{
				if (!samples.empty())
					samples += ", ";
				samples += std::format("{}:{}:{}", x, alpha(none, x), alpha(wrap, x));
			}
			throw std::runtime_error(std::format(
			    "Wrap blur must carry source energy across the right/left texture seam (none:wrap={})",
			    samples));
		}
		require(alpha(duplicate, 0) > alpha(none, 0) + 8,
		        "Duplicate blur must continue sampling the nearest edge texel");
	}

	void gaussian_blur_edge_modes_respect_input_subregion(mngl::gpu::device& device,
	                                                       mngl::two_d::renderer& renderer,
	                                                       mngl::texture& target)
	{
		const auto render_edge_mode = [&](std::string_view edge_mode)
		{
			const auto source = std::format(R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
			  <defs><filter id="tight" filterUnits="userSpaceOnUse" primitiveUnits="userSpaceOnUse"
			      x="0" y="0" width="32" height="32">
			    <feFlood x="12" y="12" width="4" height="4" flood-color="white" result="island"/>
			    <feGaussianBlur in="island" x="0" y="0" width="32" height="32"
			                    stdDeviation="3" edgeMode="{}"/>
			  </filter></defs>
			  <rect x="0" y="0" width="32" height="32" fill="black" filter="url(#tight)"/>
			  <rect x="4" y="4" width="4" height="8" fill="red"/>
			</svg>)svg", edge_mode);
			mngl::two_d::svg_object object(source);
			const mngl::view view(mngl::rectangle{glm::vec2(0), glm::vec2(target_size)});
			return render(device, renderer, target, object, view);
		};
		const auto none = render_edge_mode("none");
		const auto duplicate = render_edge_mode("duplicate");
		const auto wrap = render_edge_mode("wrap");
		const bool flipped = row_is_flipped(none);
		const auto alpha = [&](const auto& pixels, std::uint32_t x, std::uint32_t y)
		{ return pixel_at_world(pixels, x, y, flipped)[3]; };
		const auto check_mode = [&](std::string_view name, const auto& pixels)
		{
			if (alpha(pixels, 11, 14) <= alpha(none, 11, 14) + 20)
				throw std::runtime_error(std::format(
				    "{} blur must use the input subregion's left edge (none={}, mode={})", name,
				    alpha(none, 11, 14), alpha(pixels, 11, 14)));
			if (alpha(pixels, 14, 11) <= alpha(none, 14, 11) + 20)
				throw std::runtime_error(
				    std::format("{} blur must use the input subregion's top edge on the second pass "
				                "(none={}, mode={})",
				                name, alpha(none, 14, 11), alpha(pixels, 14, 11)));
		};
		check_mode("duplicate", duplicate);
		check_mode("wrap", wrap);
	}
} // namespace

int main()
{
	std::optional<mngl::gpu::device> device_storage;
	try
	{
		temporary_target_budget_rejects_overflow();
		temporary_targets_reuse_with_command_ownership();
		mngl::runtime::ensure();
		device_storage.emplace();
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "SKIP: SDL GPU backend/device unavailable: " << error.what() << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << "SVG runtime setup failure: " << error.what() << '\n';
		return 1;
	}

	auto& device = *device_storage;
	if (!device.raw())
	{
		std::cerr << "SKIP: SDL GPU device creation returned an empty handle.\n";
		return 77;
	}
	try
	{
		const char* driver = SDL_GetGPUDeviceDriver(device.raw());
		const auto svg_format = mngl::two_d::detail::svg::select_svg_shader_format(device);
		std::cout << "SVG offscreen runtime: backend=" << (driver ? driver : "<unknown>")
		          << ", shader-format=" << static_cast<unsigned int>(svg_format) << '\n';

		mngl::two_d::texture_store textures(device);
		mngl::two_d::renderer renderer(device, textures, mngl::gpu::texture_format::r8g8b8a8_unorm);
		mngl::texture target(device, target_size.x, target_size.y, mngl::gpu::texture_format::r8g8b8a8_unorm);
		target.enable_hit_testing();

		std::vector<std::string> failures;
		const auto run_case = [&](const char* name, auto&& test)
		{
			try
			{
				test();
				std::cout << "PASS: " << name << '\n';
			}
			catch (const std::exception& error)
			{
				failures.push_back(std::format("{}: {}", name, error.what()));
			}
		};
		run_case("solid draw and clipped marker hits", [&] { solid_and_marker_clip(device, renderer, target); });
		run_case("marker image viewport hit clipping",
		         [&] { marker_image_hit_clipping(device, renderer, target); });
		run_case("SVG effect view changes", [&] { filtered_view_changes(device, renderer, target); });
		run_case("external command reuses SVG effect targets",
		         [&] { external_command_reuses_svg_targets(device, renderer, target); });
		run_case("gradient and opacity compositing", [&] { gradient_and_group_opacity(device, renderer, target); });
		run_case("degenerate gradients use last stop",
		         [&] { degenerate_gradients_use_last_stop(device, renderer, target); });
		run_case("small nonzero gradients remain nondegenerate",
		         [&] { small_nonzero_gradients_remain_nondegenerate(device, renderer, target); });
		run_case("equal gradient offsets choose later stop",
		         [&] { equal_gradient_offsets_choose_later_stop(device, renderer, target); });
		run_case("use instances remain independently hit-testable",
		         [&] { use_instances_hit_test(device, renderer, target); });
		run_case("gradient href templates inherit rendered stops and coordinates",
		         [&] { inherited_gradient_href_renders_like_explicit_gradient(device, renderer, target); });
		run_case("empty gradients preserve pixels and underlying hits",
		         [&] { empty_gradient_preserves_pixels_and_underlying_hit(device, renderer, target); });
		run_case("CSS pointer-events none preserves paint and passes hits through",
		         [&] { css_pointer_events_none_keeps_paint_and_passes_hits_through(device, renderer, target); });
		run_case("adjacent SVG effects resolve independent placeholder textures",
		         [&] { adjacent_effect_draws_resolve_independently(device, renderer, target); });
		run_case("zero-region SVG effect is a no-op",
		         [&] { zero_region_svg_effect_is_noop(device, renderer, target); });
		run_case("visible and culled SVG effects resolve independently",
		         [&] { visible_and_culled_svg_effects_resolve_independently(device, renderer, target); });
		run_case("worker and order-independent SVG effects resolve independently",
		         [&] { worker_recording_effects_resolve_independently(device, renderer, target); });
		run_case("repeated multicolor SVG sprite batching",
		         [&] { repeated_multicolor_sprite_batches(device, renderer, target); });
		run_case("SVG gradient color interpolation",
		         [&] { gradient_color_interpolation(device, renderer, target); });
		run_case("SVG filter primitive color interpolation",
		         [&] { filter_primitive_color_interpolation(device, renderer, target); });
		run_case("SVG filter dependency pruning preserves output pixels",
		         [&] { filter_dependency_pruning_preserves_pixels(device, renderer, target); });
		run_case("SVG sync_view preserves loaded layout and media rules",
		         [&] { sync_view_preserves_loaded_media_and_layout(device, renderer, target); });
		run_case("SVG filter primitive default subregions follow their inputs",
		         [&] { filter_primitive_default_region_follows_input(device, renderer, target); });
		run_case("clipPath hit testing", [&] { clipped_path_hit_test(device, renderer, target); });
		run_case("nested clipPath child unions intersect their local clip paths",
		         [&] { nested_clip_path_child_union_hit_test(device, renderer, target); });
		run_case("root SVG slice viewport clipping",
		         [&] { root_viewport_slice_clip(device, renderer, target); });
		run_case("nested marker and root slice hit clips intersect under transforms",
		         [&] { nested_marker_and_root_slice_hit_clips(device, renderer, target); });
		run_case("Gaussian blur wrap seam interpolation",
		         [&] { gaussian_blur_wrap_interpolates_across_seam(device, renderer, target); });
		run_case("Gaussian blur edge modes respect input primitive subregions on both passes",
		         [&] { gaussian_blur_edge_modes_respect_input_subregion(device, renderer, target); });
		if (!failures.empty())
		{
			for (const auto& failure : failures)
				std::cerr << "SVG runtime regression failure: " << failure << '\n';
			return 1;
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "SVG runtime regression failure: " << error.what() << '\n';
		return 1;
	}
}
