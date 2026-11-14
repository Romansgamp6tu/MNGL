		static void wait_for_next_frame()
		{
			const std::uint32_t fps = max_fps_limit.load(std::memory_order_relaxed);
			if (fps == 0)
				return;

			const std::uint64_t frame_interval_ns = std::max<std::uint64_t>(SDL_NS_PER_SECOND / fps, 1);
			std::uint64_t now = SDL_GetTicksNS();
			if (!frame_clock_initialized.exchange(true, std::memory_order_relaxed))
			{
				next_frame_ns = now;
			}

			while (now < next_frame_ns)
			{
				SDL_DelayNS(next_frame_ns - now);
				now = SDL_GetTicksNS();
			}

			next_frame_ns += ((now - next_frame_ns) / frame_interval_ns + 1) * frame_interval_ns;
		}

		static void update_dt()
		{
			static std::uint64_t last_time = 0;
			static bool initialized = false;
			const std::uint64_t current_time = performance_counter();
			if (!initialized) [[unlikely]]
			{
				last_time = current_time;
				initialized = true;
				dt.store(0.0, std::memory_order_relaxed);
				return;
			}
			dt.store((current_time - last_time) / static_cast<double>(performance_frequency()),
			         std::memory_order_relaxed);
			last_time = current_time;
		}

