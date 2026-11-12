#define APP_NAME "MNGL GPU swapchain lifetime test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/render_window.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	class window_claim
	{
	public:
		window_claim(const mngl::gpu::device& device, mngl::window& target) : device(device), target(target)
		{
			device.claim_window(target);
		}

		~window_claim() { device.release_window(target); }

		window_claim(const window_claim&) = delete;
		window_claim& operator=(const window_claim&) = delete;

	private:
		const mngl::gpu::device& device;
		mngl::window& target;
	};

	bool test_explicit_cancel_after_acquire(const mngl::gpu::device& device)
	{
		mngl::window::flags flags{};
		flags.hidden = true;
		mngl::window target("MNGL swapchain cancel test", {64, 64}, flags);
		window_claim claim(device, target);

		{
			mngl::gpu::command_buffer commands(device);
			auto swapchain_texture = commands.acquire_swapchain_texture(target);
			if (!swapchain_texture.valid())
			{
				return false;
			}

			bool cancel_rejected = false;
			try
			{
				commands.cancel();
			}
			catch (const std::logic_error&)
			{
				cancel_rejected = true;
			}
			require(cancel_rejected, "cancel() must reject an acquired swapchain texture");
			// Leaving scope exercises the destructor's SDL-safe submit fallback.
		}
		device.wait();
		return true;
	}

	bool test_abandoned_render_window_frame()
	{
		mngl::window::flags flags{};
		flags.hidden = true;
		mngl::render_window target("MNGL abandoned frame test", {64, 64}, flags);

		{
			auto abandoned_frame = target.frame();
			if (!target.swapchain_texture().valid())
			{
				return false;
			}
		}

		{
			auto next_frame = target.frame();
			if (!target.swapchain_texture().valid())
			{
				return false;
			}
			next_frame.display();
		}
		return true;
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		const auto& device = mngl::gpu::device::global();
		if (!test_explicit_cancel_after_acquire(device))
		{
			std::cerr << "SDL did not provide a drawable swapchain texture.\n";
			return 77;
		}
		if (!test_abandoned_render_window_frame())
		{
			std::cerr << "SDL did not provide a drawable render_window texture.\n";
			return 77;
		}
		return 0;
	}
	catch (const mngl::sdl_error& error)
	{
		std::cerr << "No usable SDL GPU/window backend on this runner: " << error.what() << '\n';
		return 77;
	}
	catch (const std::exception& error)
	{
		std::cerr << "GPU swapchain lifetime regression test failed: " << error.what() << '\n';
		return 1;
	}
}
