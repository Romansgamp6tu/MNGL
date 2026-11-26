#define APP_NAME "MNGL SVG stroke overlap test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/svg_sprite.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	std::vector<std::uint8_t> read_pixels(const mngl::gpu::device& device, const mngl::texture& target)
	{
		const auto size = target.size();
		SDL_GPUTransferBufferCreateInfo create_info{};
		create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		create_info.size = size.x * size.y * 4;
		auto* handle = SDL_CreateGPUTransferBuffer(device.raw(), &create_info);
		mngl::check_sdl(handle != nullptr, "Failed to create SVG stroke test readback buffer");
		auto release = [raw_device = device.raw()](SDL_GPUTransferBuffer* buffer)
		{
			if (buffer)
				SDL_ReleaseGPUTransferBuffer(raw_device, buffer);
		};
		std::unique_ptr<SDL_GPUTransferBuffer, decltype(release)> transfer{handle, release};

		mngl::gpu::command_buffer commands(device);
		auto* copy_pass = SDL_BeginGPUCopyPass(commands.raw());
		mngl::check_sdl(copy_pass != nullptr, "Failed to begin SVG stroke test readback");
		SDL_GPUTextureRegion source_region{};
		source_region.texture = target.raw_gpu_texture().raw();
		source_region.w = size.x;
		source_region.h = size.y;
		source_region.d = 1;
		SDL_GPUTextureTransferInfo destination{};
		destination.transfer_buffer = transfer.get();
		destination.pixels_per_row = size.x;
		destination.rows_per_layer = size.y;
		SDL_DownloadFromGPUTexture(copy_pass, &source_region, &destination);
		SDL_EndGPUCopyPass(copy_pass);
		commands.submit();
		device.wait();

		void* mapped = SDL_MapGPUTransferBuffer(device.raw(), transfer.get(), false);
		mngl::check_sdl(mapped != nullptr, "Failed to map SVG stroke test readback buffer");
		std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size.x) * size.y * 4);
		std::memcpy(pixels.data(), mapped, pixels.size());
		SDL_UnmapGPUTransferBuffer(device.raw(), transfer.get());
		return pixels;
	}

	std::uint8_t alpha_at(const std::vector<std::uint8_t>& pixels, glm::uvec2 size, glm::uvec2 pixel)
	{
		const auto offset = (static_cast<std::size_t>(pixel.y) * size.x + pixel.x) * 4 + 3;
		return pixels.at(offset);
	}

	std::array<std::uint8_t, 4> rgba_at(const std::vector<std::uint8_t>& pixels, glm::uvec2 size,
	                                   glm::uvec2 pixel)
	{
		const auto offset = (static_cast<std::size_t>(pixel.y) * size.x + pixel.x) * 4;
		return {pixels.at(offset), pixels.at(offset + 1), pixels.at(offset + 2), pixels.at(offset + 3)};
	}

	std::vector<std::uint8_t> render_svg(const mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                                     mngl::texture& target, std::string_view source, bool resident)
	{
		mngl::two_d::svg_load_options options{};
		options.hit_testable = true;
		mngl::two_d::svg_object object(source, options);
		if (resident)
		{
			object.upload(device);
			mngl::two_d::svg_sprite sprite(object);
			auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
			frame.draw(sprite);
			frame.finish();
		}
		else
		{
			auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
			frame.draw(object);
			frame.finish();
		}
		return read_pixels(device, target);
	}

	void require_uniform_stroke_alpha(const std::vector<std::uint8_t>& pixels, glm::uvec2 size,
	                                  std::span<const glm::uvec2> samples, const char* label)
	{
		std::uint8_t reference = 0;
		for (std::size_t i = 0; i < samples.size(); ++i)
		{
			const auto alpha = alpha_at(pixels, size, samples[i]);
			require(alpha > 0, "Every stroke sample must be covered");
			if (i == 0)
				reference = alpha;
			else
				require(alpha == reference, label);
		}
		require(std::abs(static_cast<int>(reference) - 128) <= 1,
		        "A half-opacity stroke should composite once to 128 alpha");
	}

	void verify_stroke(const mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                   mngl::texture& target, std::string_view source, std::span<const glm::uvec2> samples,
	                   const char* label)
	{
		const glm::uvec2 size{target.size().x, target.size().y};
		const auto cpu_pixels = render_svg(device, renderer, target, source, false);
		const auto sprite_pixels = render_svg(device, renderer, target, source, true);
		require_uniform_stroke_alpha(cpu_pixels, size, samples, label);
		require_uniform_stroke_alpha(sprite_pixels, size, samples, label);
		for (const auto sample : samples)
		{
			const auto cpu_alpha = alpha_at(cpu_pixels, size, sample);
			const auto sprite_alpha = alpha_at(sprite_pixels, size, sample);
			if (std::abs(static_cast<int>(cpu_alpha) - static_cast<int>(sprite_alpha)) > 1)
				throw std::runtime_error(std::format(
				    "CPU-submitted geometry and svg_sprite differ at ({}, {}): {} vs {}",
				    sample.x, sample.y, cpu_alpha, sprite_alpha));
		}
	}

	std::optional<mngl::gpu::device> create_test_device()
	{
		try
		{
			mngl::runtime::ensure();
			return mngl::gpu::device{};
		}
		catch (const mngl::sdl_error& error)
		{
			std::cerr << "No usable SDL GPU runtime on this runner: " << error.what() << '\n';
			return std::nullopt;
		}
	}

	void overlapping_strokes(const mngl::gpu::device& device)
	{
		mngl::two_d::texture_store textures(device);
		mngl::two_d::renderer renderer(device, textures, mngl::gpu::texture_format::r8g8b8a8_unorm);
		mngl::texture target(device, 64, 64, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                     mngl::texture::usage::sampler | mngl::texture::usage::color_target);
		target.set_view(mngl::view(mngl::rectangle{0, 0, 64, 64}));

		constexpr std::string_view corner = R"svg(<svg width="64" height="64">
		    <path d="M8 16H32V42" fill="none" stroke="red" stroke-width="12" stroke-opacity=".5"/>
		</svg>)svg";
		constexpr std::array corner_samples{glm::uvec2{12, 12}, glm::uvec2{28, 16}, glm::uvec2{34, 30}};
		verify_stroke(device, renderer, target, corner, corner_samples,
		              "The straight segment, join, and second segment must have uniform alpha");

		constexpr std::string_view crossing = R"svg(<svg width="64" height="64">
		    <path d="M8 8L40 40L8 40L40 8" fill="none" stroke="blue" stroke-width="8" stroke-opacity=".5"/>
		</svg>)svg";
		constexpr std::array crossing_samples{glm::uvec2{12, 12}, glm::uvec2{24, 38}, glm::uvec2{24, 24}};
		verify_stroke(device, renderer, target, crossing, crossing_samples,
		              "The crossing must not accumulate alpha over the straight stroke");
	}

	void repositioned_sprite(const mngl::gpu::device& device)
	{
		mngl::two_d::texture_store textures(device);
		mngl::two_d::renderer renderer(device, textures, mngl::gpu::texture_format::r8g8b8a8_unorm);
		mngl::texture target(device, 64, 64, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                     mngl::texture::usage::sampler | mngl::texture::usage::color_target);
		target.set_view(mngl::view(mngl::rectangle{0, 0, 64, 64}));

		mngl::two_d::svg_object object(
		    R"svg(<svg width="16" height="16"><rect width="16" height="16" fill="red"/></svg>)svg");
		object.upload(device);
		mngl::two_d::svg_sprite sprite(object);
		sprite.position({8, 8}).position({24, 24});

		auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		frame.draw(sprite);
		frame.finish();

		const auto pixels = read_pixels(device, target);
		const glm::uvec2 size{target.size().x, target.size().y};
		require(alpha_at(pixels, size, {4, 4}) == 0,
		        "Repositioning an SVG sprite must clear its previous location");
		require(alpha_at(pixels, size, {28, 28}) > 0,
		        "Repositioning an SVG sprite must update its rendered geometry");
	}

	void many_gradient_stops_use_later_equal_offset(const mngl::gpu::device& device)
	{
		std::string source = R"svg(<svg width="64" height="64">
		    <defs><linearGradient id="many" gradientUnits="userSpaceOnUse" x1=".5" x2="64.5">
		        <stop offset="0" stop-color="#010101"/>)svg";
		for (std::uint32_t index = 1; index <= 32; ++index)
		{
			const float offset = static_cast<float>(index) / 32.0f;
			const char* color = index == 20 ? "#ff0000" : index == 21 ? "#0000ff" :
			                    index == 22 ? "#ffffff" : "#010101";
			source += std::format("<stop offset=\"{:.8f}\" stop-color=\"{}\"/>", offset, color);
			if (index == 21)
				source += std::format("<stop offset=\"{:.8f}\" stop-color=\"#00ff00\"/>", offset);
		}
		source += R"svg(</linearGradient></defs>
		    <rect width="64" height="64" fill="url(#many)"/>
		</svg>)svg";

		mngl::two_d::texture_store textures(device);
		mngl::two_d::renderer renderer(device, textures, mngl::gpu::texture_format::r8g8b8a8_unorm);
		mngl::texture target(device, 64, 64, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                     mngl::texture::usage::sampler | mngl::texture::usage::color_target);
		target.set_view(mngl::view(mngl::rectangle{0, 0, 64, 64}));
		const glm::uvec2 size{64, 64};
		for (const bool resident : std::array{false, true})
		{
			const auto pixels = render_svg(device, renderer, target, source, resident);
			const auto before = rgba_at(pixels, size, {41, 32});
			const auto exact = rgba_at(pixels, size, {42, 32});
			const auto after = rgba_at(pixels, size, {43, 32});
			require(before[0] > 80 && before[2] > 80 && before[1] < 32 && before[3] > 240,
			        "A many-stop gradient must interpolate into the first color at a repeated offset");
			require(exact[1] > 220 && exact[0] < 32 && exact[2] < 32 && exact[3] > 240,
			        "At a repeated offset, a many-stop gradient must select the later stop");
			require(after[0] > 80 && after[2] > 80 && after[1] > 220 && after[3] > 240,
			        "After a repeated offset, interpolation must continue from the later stop");
		}
	}
} // namespace

int main()
{
	try
	{
		auto device = create_test_device();
		if (!device)
			return 77;
		overlapping_strokes(*device);
		repositioned_sprite(*device);
		many_gradient_stops_use_later_equal_offset(*device);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
