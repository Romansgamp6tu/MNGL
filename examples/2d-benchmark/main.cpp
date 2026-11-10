#include "mngl.hpp"

#include <SDL3/SDL_gpu.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	using clock_type = std::chrono::steady_clock;
	constexpr std::size_t sample_capacity = 120;

	enum class scene_mode
	{
		sprites,
		geometry,
		mixed
	};

	enum class geometry_storage_mode
	{
		resident,
		dynamic
	};

	struct options
	{
		std::size_t sprite_count = 20000;
		std::size_t triangle_count = 20000;
		std::uint32_t offscreen_percent = 70;
		scene_mode mode = scene_mode::mixed;
		geometry_storage_mode geometry_storage = geometry_storage_mode::resident;
		bool gpu_culling = false;
		bool no_present = false;
		bool wait_for_gpu = false;
	};

	struct scene_data
	{
		std::vector<mngl::sprite> sprites;
		mngl::two_d::vertex_array geometry{mngl::two_d::primitive_type::triangles};
		std::unique_ptr<mngl::two_d::mesh> resident_geometry;
		glm::uvec2 pixel_size{};
	};

	struct frame_sample
	{
		double loop_ms = 0.0;
		double input_ms = 0.0;
		double begin_ms = 0.0;
		double draw_ms = 0.0;
		double display_ms = 0.0;
		double recording_wall_ms = 0.0;
		double culling_cpu_ms = 0.0;
		double upload_cpu_ms = 0.0;
		double command_encoding_cpu_ms = 0.0;
		double flush_cpu_ms = 0.0;
		double gpu_idle_wait_ms = 0.0;
	};

	struct distribution
	{
		double average = 0.0;
		double percentile_95 = 0.0;
	};

	[[nodiscard]] std::size_t parse_count(std::string_view text, std::string_view option)
	{
		std::size_t value = 0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
		if (error != std::errc{} || end != text.data() + text.size())
		{
			throw std::invalid_argument(std::format("{} expects a non-negative integer", option));
		}
		return value;
	}

	[[nodiscard]] options parse_options(int argc, char** argv)
	{
		options result;
		for (int index = 1; index < argc; ++index)
		{
			const std::string_view argument(argv[index]);
			if (argument == "--help")
			{
				std::cout << "mngl_2d_benchmark [--sprites COUNT] [--triangles COUNT] [--offscreen 0..100] "
				             "[--mode sprites|geometry|mixed] [--geometry-storage resident|dynamic] "
				             "[--gpu-culling on|off] [--no-present] [--wait-gpu]\n";
				std::cout << "Keys: 1=sprites, 2=geometry, 3=mixed, F2=toggle GPU culling, Escape=quit\n";
				std::cout << "--wait-gpu renders offscreen and waits for GPU completion after each frame.\n";
				std::exit(0);
			}
			if (argument == "--no-present")
			{
				result.no_present = true;
				continue;
			}
			if (argument == "--wait-gpu")
			{
				result.no_present = true;
				result.wait_for_gpu = true;
				continue;
			}
			if (index + 1 >= argc)
			{
				throw std::invalid_argument(std::format("{} expects a value", argument));
			}

			const std::string_view option_value(argv[++index]);
			if (argument == "--sprites")
			{
				const std::size_t value = parse_count(option_value, argument);
				if (value > 1'000'000)
				{
					throw std::invalid_argument("--sprites must be at most 1000000");
				}
				result.sprite_count = value;
			}
			else if (argument == "--triangles")
			{
				const std::size_t value = parse_count(option_value, argument);
				if (value > 2'000'000)
				{
					throw std::invalid_argument("--triangles must be at most 2000000");
				}
				result.triangle_count = value;
			}
			else if (argument == "--offscreen")
			{
				const std::size_t value = parse_count(option_value, argument);
				if (value > 100)
				{
					throw std::invalid_argument("--offscreen must be between 0 and 100");
				}
				result.offscreen_percent = static_cast<std::uint32_t>(value);
			}
			else if (argument == "--mode")
			{
				if (option_value == "sprites")
				{
					result.mode = scene_mode::sprites;
				}
				else if (option_value == "geometry")
				{
					result.mode = scene_mode::geometry;
				}
				else if (option_value == "mixed")
				{
					result.mode = scene_mode::mixed;
				}
				else
				{
					throw std::invalid_argument("--mode must be sprites, geometry, or mixed");
				}
			}
			else if (argument == "--gpu-culling")
			{
				if (option_value == "on")
				{
					result.gpu_culling = true;
				}
				else if (option_value == "off")
				{
					result.gpu_culling = false;
				}
				else
				{
					throw std::invalid_argument("--gpu-culling must be on or off");
				}
			}
			else if (argument == "--geometry-storage")
			{
				if (option_value == "resident")
				{
					result.geometry_storage = geometry_storage_mode::resident;
				}
				else if (option_value == "dynamic")
				{
					result.geometry_storage = geometry_storage_mode::dynamic;
				}
				else
				{
					throw std::invalid_argument("--geometry-storage must be resident or dynamic");
				}
			}
			else
			{
				throw std::invalid_argument(std::format("Unknown option: {}", argument));
			}
		}
		return result;
	}

	[[nodiscard]] float random_unit(std::uint32_t& state) noexcept
	{
		state = state * 1664525u + 1013904223u;
		return static_cast<float>(state >> 8) * (1.0f / 16777216.0f);
	}

	void build_scene(mngl::render_window& window, const mngl::gpu::device& device,
	                 const options& configuration, scene_data& scene)
	{
		scene.sprites.clear();
		scene.sprites.reserve(configuration.sprite_count);
		scene.resident_geometry.reset();
		scene.geometry = mngl::two_d::vertex_array{mngl::two_d::primitive_type::triangles};
		scene.geometry.vertices.reserve(configuration.triangle_count * 3);
		scene.pixel_size = window.pixel_size();

		const glm::vec2 extent = window.coordinate_size();
		if (extent.x <= 0.0f || extent.y <= 0.0f)
		{
			return;
		}

		const auto white_region = window.textures().white_region();
		std::uint32_t random_state = 0x4D4E474Cu;
		for (std::size_t index = 0; index < configuration.sprite_count; ++index)
		{
			float x = random_unit(random_state) * extent.x;
			const float y = random_unit(random_state) * extent.y;
			if (index % 100 < configuration.offscreen_percent)
			{
				x += extent.x * 2.0f;
			}
			const float width = 0.004f + random_unit(random_state) * 0.010f;
			const float height = 0.004f + random_unit(random_state) * 0.010f;
			const mngl::color tint{0.2f + random_unit(random_state) * 0.8f,
			                       0.2f + random_unit(random_state) * 0.8f,
			                       0.2f + random_unit(random_state) * 0.8f, 1.0f};
			scene.sprites.emplace_back(white_region, mngl::rectangle{x, y, width, height}, tint);
		}

		scene.geometry.tint = {0.25f, 0.7f, 0.95f, 1.0f};
		scene.geometry.blend = mngl::gpu::sprite_blend_mode::opaque;
		for (std::size_t index = 0; index < configuration.triangle_count; ++index)
		{
			float center_x = random_unit(random_state) * extent.x;
			const float center_y = random_unit(random_state) * extent.y;
			if (index % 100 < configuration.offscreen_percent)
			{
				center_x += extent.x * 2.0f;
			}
			const float half_width = 0.003f + random_unit(random_state) * 0.010f;
			const float half_height = 0.003f + random_unit(random_state) * 0.010f;
			scene.geometry.vertices.emplace_back(center_x - half_width, center_y - half_height);
			scene.geometry.vertices.emplace_back(center_x + half_width, center_y - half_height);
			scene.geometry.vertices.emplace_back(center_x, center_y + half_height);
		}

		if (configuration.geometry_storage == geometry_storage_mode::resident &&
		    !scene.geometry.vertices.empty())
		{
			mngl::gpu::command_buffer upload_commands(device);
			scene.resident_geometry = std::make_unique<mngl::two_d::mesh>(
			    device, upload_commands, scene.geometry);
			upload_commands.submit();
			scene.geometry.vertices.clear();
			scene.geometry.vertices.shrink_to_fit();
		}
	}

	[[nodiscard]] double milliseconds(std::chrono::nanoseconds value) noexcept
	{
		return std::chrono::duration<double, std::milli>(value).count();
	}

	template <typename Select>
	[[nodiscard]] distribution summarize(const std::array<frame_sample, sample_capacity>& samples,
	                                     std::size_t count, Select select)
	{
		std::array<double, sample_capacity> values{};
		double sum = 0.0;
		for (std::size_t index = 0; index < count; ++index)
		{
			values[index] = select(samples[index]);
			sum += values[index];
		}
		std::sort(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(count));
		const std::size_t percentile_index = count == 0 ? 0 : ((count - 1) * 95) / 100;
		return {count == 0 ? 0.0 : sum / static_cast<double>(count), values[percentile_index]};
	}

	[[nodiscard]] std::string_view mode_name(scene_mode mode) noexcept
	{
		switch (mode)
		{
		case scene_mode::sprites:
			return "sprites";
		case scene_mode::geometry:
			return "geometry";
		case scene_mode::mixed:
			return "mixed";
		}
		return "unknown";
	}

	[[nodiscard]] std::string_view geometry_storage_name(geometry_storage_mode mode) noexcept
	{
		return mode == geometry_storage_mode::resident ? "resident" : "dynamic";
	}

	void print_report(const std::array<frame_sample, sample_capacity>& samples, std::size_t count,
	                  scene_mode mode, geometry_storage_mode geometry_storage, bool no_present,
	                  bool wait_for_gpu,
	                  const mngl::gpu::batcher2d::frame_stats& last_stats,
	                  const mngl::two_d::renderer& renderer)
	{
		const auto print = [&](std::string_view name, auto select)
		{
			const auto value = summarize(samples, count, select);
			std::cout << std::format(" {:<14} {:6.2f}/{:6.2f} ms |", name, value.average,
			                         value.percentile_95);
		};

		std::cout << std::format("\nmode={} geometry_storage={} target={} gpu_culling={} "
		                         "(average/p95 over {} frames)\n",
		                         mode_name(mode), geometry_storage_name(geometry_storage),
		                         no_present ? "offscreen" : "window",
		                         renderer.gpu_culling_enabled() ? "on" : "off", count);
		print("loop", [](const frame_sample& value) { return value.loop_ms; });
		print("input", [](const frame_sample& value) { return value.input_ms; });
		print("frame begin", [](const frame_sample& value) { return value.begin_ms; });
		print("draw calls", [](const frame_sample& value) { return value.draw_ms; });
		print(no_present ? "finish+submit" : "display",
		      [](const frame_sample& value) { return value.display_ms; });
		if (wait_for_gpu)
		{
			print("GPU idle wait", [](const frame_sample& value) { return value.gpu_idle_wait_ms; });
		}
		std::cout << '\n';
		print("record wall", [](const frame_sample& value) { return value.recording_wall_ms; });
		print("culling", [](const frame_sample& value) { return value.culling_cpu_ms; });
		print("upload", [](const frame_sample& value) { return value.upload_cpu_ms; });
		print("GPU cmd CPU", [](const frame_sample& value) { return value.command_encoding_cpu_ms; });
		print("flush", [](const frame_sample& value) { return value.flush_cpu_ms; });
		std::cout << '\n'
		          << std::format("batches={} draw_calls={} instances={} upload={} bytes\n",
		                         last_stats.batches, last_stats.draw_calls, last_stats.sprites,
		                         last_stats.upload_bytes)
		          << std::flush;
	}
} // namespace

int main(int argc, char** argv)
{
	try
	{
		const options configuration = parse_options(argc, argv);
		mngl::runtime::ensure();
		mngl::input::set_max_fps(0);

		mngl::window::flags flags;
		flags.resizable = true;
		mngl::render_window window("MNGL 2D benchmark", {1280, 720}, flags);
		const auto& device = window.gpu_device();
		if (!configuration.no_present &&
		    !SDL_WindowSupportsGPUPresentMode(device.raw(), window.raw(), SDL_GPU_PRESENTMODE_IMMEDIATE))
		{
			throw std::runtime_error("The active GPU backend does not support immediate present; cannot disable VSync");
		}
		if (!configuration.no_present &&
		    !SDL_SetGPUSwapchainParameters(device.raw(), window.raw(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
		                                   SDL_GPU_PRESENTMODE_IMMEDIATE))
		{
			throw std::runtime_error(std::format("Failed to disable VSync: {}", SDL_GetError()));
		}
		auto& renderer = window.renderer();
		renderer.enable_profiling();
		renderer.enable_gpu_culling(configuration.gpu_culling);

		scene_data scene;
		build_scene(window, device, configuration, scene);
		mngl::texture offscreen_target;
		if (configuration.no_present)
		{
			const auto size = window.pixel_size();
			if (size.x == 0 || size.y == 0) [[unlikely]]
			{
				throw std::runtime_error("Cannot create the benchmark target for a zero-sized window");
			}
			offscreen_target = window.create_texture(size, mngl::gpu::texture_format::invalid,
			                                        mngl::texture::usage::color_target);
		}
		scene_mode mode = configuration.mode;
		std::array<frame_sample, sample_capacity> samples{};
		std::size_t sample_count = 0;
		std::size_t sample_cursor = 0;
		std::size_t warmup_frames = sample_capacity;
		auto next_report = clock_type::now() + std::chrono::seconds(1);

		std::cout << "FPS limit: off. ";
		if (configuration.no_present)
		{
			std::cout << "Target: offscreen; swapchain present: off. ";
		}
		else
		{
			std::cout << "VSync: off (immediate present). ";
		}
		if (configuration.wait_for_gpu)
		{
			std::cout << "GPU completion wait: on (host wait, not a hardware timestamp). ";
		}
		std::cout << "Warm-up: 120 frames per mode. Keys: 1=sprites, 2=geometry, 3=mixed, "
		             "F2=GPU culling, Escape=quit.\n"
		          << std::flush;

		for (;;)
		{
			const auto loop_started_at = clock_type::now();
			const auto input_started_at = loop_started_at;
			if (!mngl::input::next_frame())
			{
				break;
			}
			const auto input_time =
			    std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - input_started_at);

			bool mode_changed = false;
			for (const auto& keyboard : mngl::input::keyboards())
			{
				if (keyboard.pressed(mngl::scancode::escape))
				{
					mngl::input::should_work.store(false, std::memory_order_release);
				}
				if (keyboard.pressed(mngl::scancode::num1))
				{
					mode = scene_mode::sprites;
					mode_changed = true;
				}
				if (keyboard.pressed(mngl::scancode::num2))
				{
					mode = scene_mode::geometry;
					mode_changed = true;
				}
				if (keyboard.pressed(mngl::scancode::num3))
				{
					mode = scene_mode::mixed;
					mode_changed = true;
				}
				if (keyboard.pressed(mngl::scancode::f2))
				{
					renderer.enable_gpu_culling(!renderer.gpu_culling_enabled());
					mode_changed = true;
				}
			}
			if (mode_changed)
			{
				sample_count = 0;
				sample_cursor = 0;
				warmup_frames = sample_capacity;
			}

			const auto target_size = window.pixel_size();
			if (target_size.x == 0 || target_size.y == 0)
			{
				continue;
			}
			if (scene.pixel_size != target_size)
			{
				build_scene(window, device, configuration, scene);
				if (configuration.no_present)
				{
					offscreen_target = window.create_texture(target_size, mngl::gpu::texture_format::invalid,
					                                        mngl::texture::usage::color_target);
				}
				warmup_frames = sample_capacity;
				sample_count = 0;
				sample_cursor = 0;
			}

			const auto begin_started_at = clock_type::now();
			auto frame = configuration.no_present ? window.frame(offscreen_target) : window.frame();
			const auto begin_time =
			    std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - begin_started_at);

			const auto draw_started_at = clock_type::now();
			if (mode != scene_mode::geometry)
			{
				for (const auto& value : scene.sprites)
				{
					frame.draw(value);
				}
			}
			if (mode != scene_mode::sprites)
			{
				if (scene.resident_geometry)
				{
					frame.draw(*scene.resident_geometry);
				}
				else
				{
					frame.draw(scene.geometry);
				}
			}
			const auto draw_time =
			    std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - draw_started_at);

			const auto display_started_at = clock_type::now();
			frame.display();
			const auto display_time =
			    std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - display_started_at);
			std::chrono::nanoseconds gpu_idle_wait_time{};
			if (configuration.wait_for_gpu)
			{
				const auto wait_started_at = clock_type::now();
				device.wait();
				gpu_idle_wait_time =
				    std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - wait_started_at);
			}
			const auto loop_time =
			    std::chrono::duration_cast<std::chrono::nanoseconds>(clock_type::now() - loop_started_at);

			if (warmup_frames != 0)
			{
				--warmup_frames;
				continue;
			}

			const auto& stats = renderer.last_frame_stats();
			frame_sample sample;
			sample.loop_ms = milliseconds(loop_time);
			sample.input_ms = milliseconds(input_time);
			sample.begin_ms = milliseconds(begin_time);
			sample.draw_ms = milliseconds(draw_time);
			sample.display_ms = milliseconds(display_time);
			sample.recording_wall_ms = milliseconds(stats.frame_recording_wall_time);
			sample.culling_cpu_ms = milliseconds(stats.culling_cpu_time);
			sample.upload_cpu_ms = milliseconds(stats.upload_cpu_time);
			sample.command_encoding_cpu_ms = milliseconds(stats.command_encoding_cpu_time);
			sample.flush_cpu_ms = milliseconds(stats.flush_cpu_time);
			sample.gpu_idle_wait_ms = milliseconds(gpu_idle_wait_time);
			samples[sample_cursor] = sample;
			sample_cursor = (sample_cursor + 1) % sample_capacity;
			sample_count = std::min(sample_count + 1, sample_capacity);

			if (sample_count == sample_capacity && clock_type::now() >= next_report)
			{
				print_report(samples, sample_count, mode, configuration.geometry_storage,
				             configuration.no_present, configuration.wait_for_gpu, stats, renderer);
				next_report = clock_type::now() + std::chrono::seconds(1);
			}
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
