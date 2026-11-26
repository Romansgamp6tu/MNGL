#define APP_NAME "MNGL SVG upload lifecycle test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/svg_sprite.hpp>
#include <MNGL/gpu_buffer.hpp>
#include <MNGL/render_pass.hpp>
#include <MNGL/text.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	constexpr std::string_view source = R"svg(<svg width="64" height="64" viewBox="0 0 64 64">
	    <rect x="2" y="2" width="24" height="24" fill="red"/>
	    <image x="30" y="2" width="2" height="1"
	        href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAYAAAD0In+KAAAADklEQVR4nGP4z8AAQg0AD3oDfnfpf5cAAAAASUVORK5CYII="/>
	</svg>)svg";

	std::vector<std::uint8_t> read_pixels(const mngl::gpu::device& device, const mngl::texture& target)
	{
		const auto size = target.size();
		SDL_GPUTransferBufferCreateInfo create_info{};
		create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		create_info.size = size.x * size.y * 4;
		auto* handle = SDL_CreateGPUTransferBuffer(device.raw(), &create_info);
		mngl::check_sdl(handle != nullptr, "Failed to create SVG test readback buffer");
		auto release = [raw_device = device.raw()](SDL_GPUTransferBuffer* buffer)
		{
			if (buffer)
				SDL_ReleaseGPUTransferBuffer(raw_device, buffer);
		};
		std::unique_ptr<SDL_GPUTransferBuffer, decltype(release)> transfer{handle, release};

		mngl::gpu::command_buffer commands(device);
		auto* copy_pass = SDL_BeginGPUCopyPass(commands.raw());
		mngl::check_sdl(copy_pass != nullptr, "Failed to begin SVG test texture readback");
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
		mngl::check_sdl(mapped != nullptr, "Failed to map SVG test readback buffer");
		std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size.x) * size.y * 4);
		std::memcpy(pixels.data(), mapped, pixels.size());
		SDL_UnmapGPUTransferBuffer(device.raw(), transfer.get());
		return pixels;
	}

	std::size_t nontransparent_pixels(const std::vector<std::uint8_t>& pixels)
	{
		std::size_t result = 0;
		for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4)
			result += pixels[alpha] != 0;
		return result;
	}

	std::uint8_t alpha_at(const std::vector<std::uint8_t>& pixels, std::uint32_t width, std::uint32_t x,
	                      std::uint32_t y)
	{
		const auto offset = (static_cast<std::size_t>(y) * width + x) * 4 + 3;
		return pixels.at(offset);
	}

	std::vector<std::uint32_t> read_buffer(const mngl::gpu::device& device,
	                                       const mngl::gpu::buffer& source)
	{
		SDL_GPUTransferBufferCreateInfo create_info{};
		create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		create_info.size = source.size();
		auto* handle = SDL_CreateGPUTransferBuffer(device.raw(), &create_info);
		mngl::check_sdl(handle != nullptr, "Failed to create GPU buffer readback storage");
		auto release = [raw_device = device.raw()](SDL_GPUTransferBuffer* buffer)
		{
			if (buffer)
				SDL_ReleaseGPUTransferBuffer(raw_device, buffer);
		};
		std::unique_ptr<SDL_GPUTransferBuffer, decltype(release)> transfer{handle, release};

		mngl::gpu::command_buffer commands(device);
		auto* copy_pass = SDL_BeginGPUCopyPass(commands.raw());
		mngl::check_sdl(copy_pass != nullptr, "Failed to begin GPU buffer readback");
		const SDL_GPUBufferRegion source_region{.buffer = source.raw(), .offset = 0, .size = source.size()};
		const SDL_GPUTransferBufferLocation destination{.transfer_buffer = transfer.get(), .offset = 0};
		SDL_DownloadFromGPUBuffer(copy_pass, &source_region, &destination);
		SDL_EndGPUCopyPass(copy_pass);
		commands.submit();
		device.wait();

		void* mapped = SDL_MapGPUTransferBuffer(device.raw(), transfer.get(), false);
		mngl::check_sdl(mapped != nullptr, "Failed to map GPU buffer readback storage");
		std::vector<std::uint32_t> values(source.size() / sizeof(std::uint32_t));
		std::memcpy(values.data(), mapped, source.size());
		SDL_UnmapGPUTransferBuffer(device.raw(), transfer.get());
		return values;
	}

	void partial_buffer_updates_preserve_contents(const mngl::gpu::device& device)
	{
		const std::array<std::uint32_t, 4> original{11, 22, 33, 44};
		mngl::gpu::buffer buffer(device, sizeof(original), mngl::gpu::buffer_usage::graphics_storage_read);
		mngl::gpu::command_buffer initial_upload(device);
		buffer.upload(device, initial_upload, std::span<const std::uint32_t>(original));
		initial_upload.submit();

		const std::array<std::uint32_t, 1> canceled_value{99};
		mngl::gpu::command_buffer canceled_update(device);
		buffer.upload_range(device, canceled_update, 1, std::span<const std::uint32_t>(canceled_value));
		canceled_update.cancel();
		require(read_buffer(device, buffer) == std::vector<std::uint32_t>(original.begin(), original.end()),
		        "Canceling a partial buffer update must preserve all submitted bytes");

		const std::array<std::uint32_t, 1> first_value{55};
		const std::array<std::uint32_t, 1> last_value{66};
		mngl::gpu::command_buffer submitted_updates(device);
		buffer.upload_range(device, submitted_updates, 0, std::span<const std::uint32_t>(first_value));
		buffer.upload_range(device, submitted_updates, 3, std::span<const std::uint32_t>(last_value));
		submitted_updates.submit();
		require(read_buffer(device, buffer) == std::vector<std::uint32_t>{55, 22, 33, 66},
		        "Submitted partial buffer updates must retain untouched bytes and apply both ranges");
	}

	void texture_upload_rejects_active_pass(const mngl::gpu::device& device)
	{
		mngl::texture render_target(device, 8, 8, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                           mngl::texture::usage::color_target);
		mngl::texture upload_target(device, 8, 8, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                           mngl::texture::usage::sampler);
		const std::array<std::uint8_t, 8 * 8 * 4> pixels{};
		mngl::gpu::command_buffer commands(device);
		mngl::gpu::render_pass pass(commands, render_target.raw_gpu_texture());
		bool rejected = false;
		try
		{
			upload_target.update_pixels(commands, std::span<const std::uint8_t>(pixels));
		}
		catch (const std::logic_error&)
		{
			rejected = true;
		}
		require(rejected, "Texture uploads must be rejected while another GPU pass is active");
		rejected = false;
		try
		{
			commands.submit();
		}
		catch (const std::logic_error&)
		{
			rejected = true;
		}
		require(rejected && commands.valid(),
		        "A command buffer cannot submit while a GPU pass still owns it");
		rejected = false;
		try
		{
			commands.cancel();
		}
		catch (const std::logic_error&)
		{
			rejected = true;
		}
		require(rejected && commands.valid(),
		        "A command buffer cannot cancel while a GPU pass still owns it");
		pass.end_pass();
		upload_target.update_pixels(commands, std::span<const std::uint8_t>(pixels));
		commands.submit();
	}

	void texture_upload_reuses_staging(const mngl::gpu::device& device)
	{
		mngl::texture target(device, 8, 8, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                     mngl::texture::usage::sampler | mngl::texture::usage::color_target);
		std::array<std::uint8_t, 8 * 8 * 4> first{};
		std::array<std::uint8_t, 8 * 8 * 4> second{};
		for (std::size_t index = 0; index < second.size(); index += 4)
		{
			second[index] = 17;
			second[index + 1] = 34;
			second[index + 2] = 51;
			second[index + 3] = 255;
		}
		target.update_pixels(device, std::span<const std::uint8_t>(first));
		target.update_pixels(device, std::span<const std::uint8_t>(second));
		const auto pixels = read_pixels(device, target);
		require(pixels == std::vector<std::uint8_t>(second.begin(), second.end()),
		        "Repeated same-size texture updates must safely reuse transfer staging");
	}

	void external_frame_hit_tests_commit_on_submission(const mngl::gpu::device& device,
	                                                   mngl::two_d::renderer& renderer,
	                                                   mngl::texture& target)
	{
		target.enable_hit_testing();
		const mngl::two_d::rectangle_shape left({4, 4, 18, 18}, mngl::color(1, 0, 0, 1));
		const mngl::two_d::rectangle_shape right({40, 4, 18, 18}, mngl::color(0, 1, 0, 1));
		auto first = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		first.draw(left);
		first.finish();
		require(target.hit_test({10, 10}).has_value(), "A submitted frame must publish its hit-test geometry");

		mngl::gpu::command_buffer canceled_commands(device);
		auto canceled = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		canceled.draw(right);
		canceled.finish(canceled_commands);
		require(target.hit_test({10, 10}).has_value() && !target.hit_test({49, 10}),
		        "Recording an external frame must not replace the completed hit-test snapshot");
		canceled_commands.cancel();
		require(target.hit_test({10, 10}).has_value() && !target.hit_test({49, 10}),
		        "Canceling an external frame must preserve the prior hit-test snapshot");

		mngl::gpu::command_buffer submitted_commands(device);
		auto submitted = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		submitted.draw(right);
		submitted.finish(submitted_commands);
		submitted_commands.submit();
		require(!target.hit_test({10, 10}) && target.hit_test({49, 10}).has_value(),
		        "Submitting an external frame must publish its new hit-test geometry");
	}

	void canceled_font_upload_retries(const mngl::gpu::device& device,
	                                 mngl::two_d::renderer& renderer)
	{
		mngl::two_d::font typeface(device, std::filesystem::path(MNGL_TEST_FONT_PATH), 32.0f, {128, 128});
		mngl::texture target(device, 64, 64, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                     mngl::texture::usage::sampler | mngl::texture::usage::color_target);
		target.set_view(mngl::view(mngl::rectangle{0, 0, 64, 64}));
		mngl::two_d::text_draw_options options{};
		options.height = 16.0f;
		options.clip_bounds = mngl::rectangle{0, 0, 64, 64};

		mngl::gpu::command_buffer canceled_upload(device);
		auto canceled = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		canceled.draw_text(typeface, "MNGL", {4, 40}, options);
		canceled.finish(canceled_upload);
		canceled_upload.cancel();

		auto retry = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		retry.draw_text(typeface, "MNGL", {4, 40}, options);
		retry.finish();
		require(nontransparent_pixels(read_pixels(device, target)) > 0,
		        "A canceled font atlas upload must be retried before the next text draw");

		auto incremental = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		incremental.draw_text(typeface, "SVG", {4, 40}, options);
		incremental.finish();
		require(nontransparent_pixels(read_pixels(device, target)) > 0,
		        "New glyphs must upload incrementally after the atlas has been submitted once");
	}

	void draw_and_check(const mngl::gpu::device& device, mngl::two_d::renderer& renderer,
	                    mngl::texture& target, const mngl::drawable& drawable, const char* failure_message)
	{
		auto frame = renderer.begin_frame(target, mngl::color(0, 0, 0, 0));
		frame.draw(drawable);
		frame.finish();
		const auto pixels = read_pixels(device, target);
		require(nontransparent_pixels(pixels) > 0, failure_message);
		require(alpha_at(pixels, target.width(), 30, 2) != 0,
		        "The embedded SVG image must produce alpha at its known interior pixel");
	}

	void upload_lifecycle()
	{
		mngl::gpu::device device;
		mngl::two_d::svg_object object(source);
		mngl::two_d::texture_store textures(device);
		mngl::two_d::renderer renderer(device, textures, mngl::gpu::texture_format::r8g8b8a8_unorm);
		mngl::texture target(device, 64, 64, mngl::gpu::texture_format::r8g8b8a8_unorm,
		                     mngl::texture::usage::sampler | mngl::texture::usage::color_target);
		target.set_view(mngl::view(mngl::rectangle{0, 0, 64, 64}));
		partial_buffer_updates_preserve_contents(device);
		texture_upload_rejects_active_pass(device);
		texture_upload_reuses_staging(device);
		external_frame_hit_tests_commit_on_submission(device, renderer, target);
		canceled_font_upload_retries(device, renderer);

		mngl::gpu::command_buffer first_upload(device);
		object.upload(device, first_upload);
		mngl::gpu::command_buffer moved_upload(std::move(first_upload));
		require(!object.gpu_resident(), "An unsubmitted first upload must not report GPU residency");
		bool pending_sprite_rejected = false;
		try
		{
			mngl::two_d::svg_sprite pending_sprite(object);
		}
		catch (const std::logic_error&)
		{
			pending_sprite_rejected = true;
		}
		require(pending_sprite_rejected, "A sprite must not retain resources from a pending first upload");
		moved_upload.cancel();
		require(!object.gpu_resident(), "Canceling a first upload must leave the CPU object non-resident");
		draw_and_check(device, renderer, target, object,
		               "A canceled first upload must preserve visible CPU drawing");

		mngl::gpu::command_buffer abandoned_upload(device);
		{
			mngl::two_d::svg_object transient(source);
			transient.upload(device, abandoned_upload);
		}
		abandoned_upload.cancel();

		object.upload(device);
		require(object.gpu_resident(), "A successful synchronous upload must commit GPU residency");
		draw_and_check(device, renderer, target, object,
		               "A successful image upload must draw visible GPU pixels");
		mngl::two_d::svg_sprite previous_sprite(object);
		require(!previous_sprite.empty(), "The uploaded SVG sprite must retain drawable geometry");

		mngl::gpu::command_buffer replacement(device);
		object.upload(device, replacement);
		require(object.gpu_resident(),
		        "A pending replacement must keep the previous resident copy available");
		mngl::two_d::svg_sprite during_replacement(object);
		require(!during_replacement.empty(), "A pending replacement must keep the old sprite drawable");
		replacement.cancel();
		require(object.gpu_resident(), "Canceling a replacement must preserve the previous GPU copy");
		draw_and_check(device, renderer, target, object,
		               "A canceled replacement must preserve visible pixels from the prior GPU copy");
		mngl::two_d::svg_sprite after_cancel(object);
		require(!after_cancel.empty(), "A canceled replacement must keep the old sprite drawable");

		mngl::gpu::command_buffer successful_replacement(device);
		object.upload(device, successful_replacement);
		successful_replacement.submit();
		require(object.gpu_resident(), "Submitting a replacement must leave the object GPU-resident");
		mngl::two_d::svg_sprite after_submit(object);
		require(!after_submit.empty(), "A submitted replacement must provide drawable sprite geometry");

		mngl::two_d::svg_object overlapping_uploads(source);
		mngl::gpu::command_buffer earlier_upload(device);
		mngl::gpu::command_buffer later_upload(device);
		overlapping_uploads.upload(device, earlier_upload);
		overlapping_uploads.upload(device, later_upload);
		later_upload.cancel();
		require(!overlapping_uploads.gpu_resident(),
		        "Canceling the newest upload must leave an earlier pending upload pending");
		bool older_upload_still_pending = false;
		try
		{
			mngl::two_d::svg_sprite pending_sprite(overlapping_uploads);
		}
		catch (const std::logic_error&)
		{
			older_upload_still_pending = true;
		}
		require(older_upload_still_pending,
		        "An earlier upload must remain pending after a newer overlapping upload is canceled");
		earlier_upload.submit();
		require(overlapping_uploads.gpu_resident(),
		        "A successful earlier upload must become resident when a newer upload was canceled");
		draw_and_check(device, renderer, target, overlapping_uploads,
		               "A successful earlier upload must remain drawable after a newer upload is canceled");

		mngl::gpu::command_buffer moved_object_upload(device);
		mngl::two_d::svg_object moved_object(source);
		moved_object.upload(device, moved_object_upload);
		mngl::two_d::svg_object destination(std::move(moved_object));
		moved_object_upload.submit();
		require(destination.gpu_resident(), "A pending upload must follow an SVG object move");

		mngl::two_d::svg_object gradient_object(R"svg(<svg width="16" height="16">
		    <defs><linearGradient id="paint"><stop stop-color="red"/><stop offset="1" stop-color="blue"/></linearGradient></defs>
		    <rect width="16" height="16" fill="url(#paint)"/>
		</svg>)svg");
		mngl::gpu::command_buffer gradient_upload(device);
		gradient_object.upload(device, gradient_upload);
		gradient_upload.cancel();
		require(!gradient_object.gpu_resident(), "Canceling a gradient upload must not mark it resident");
		gradient_object.upload(device);
		require(gradient_object.gpu_resident(), "Submitting a gradient upload must commit its mesh");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		upload_lifecycle();
		return 0;
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "No usable SDL GPU backend on this runner: " << error.what() << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
