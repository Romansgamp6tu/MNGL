#define APP_NAME "MNGL core API test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)
#include <MNGL/async_io.hpp>
#include <MNGL/dialog.hpp>
#include <MNGL/event.hpp>
#include <MNGL/hit_test.hpp>
#include <MNGL/color.hpp>
#include <MNGL/rectangle.hpp>
#include <MNGL/window.hpp>
#include <MNGL/paths.hpp>
#include <MNGL/process.hpp>
#include <MNGL/storage.hpp>
#include <MNGL/surface.hpp>

#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <iterator>
#include <string>
#include <stdexcept>
#include <thread>

namespace
{
	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	struct temporary_directory
	{
		std::filesystem::path path =
		    std::filesystem::temp_directory_path() / ("mngl-core-api-" + std::to_string(SDL_GetTicksNS()));
		temporary_directory() { std::filesystem::create_directory(path); }
		~temporary_directory()
		{
			std::error_code ignored;
			std::filesystem::remove_all(path, ignored);
		}
	};

	void test_errors_and_paths()
	{
		SDL_SetError("captured backend error");
		mngl::sdl_error captured("context: ");
		const std::string captured_message(captured.what());
		SDL_SetError("unrelated later error");
		require(std::string_view(captured.what()).starts_with("context: captured backend error") &&
		            captured_message == captured.what(),
		        "SDL error was not captured");
		auto invalid = mngl::paths::try_user_folder_path(static_cast<mngl::paths::user_folder>(-1));
		require(!invalid && std::string_view(invalid.error().what()).starts_with("invalid user folder") &&
		            std::string_view(invalid.error().what()).find("unrelated later error") ==
		                std::string_view::npos,
		        "try_ path retained stale SDL state");
		bool threw = false;
		try
		{
			(void)mngl::paths::user_folder_path(static_cast<mngl::paths::user_folder>(-1));
		}
		catch (const std::invalid_argument&)
		{
			threw = true;
		}
		require(threw, "invalid folder did not reject the argument");
		auto application = mngl::paths::try_application_directory();
		require(application && *application == mngl::paths::application_directory(), "path APIs disagree");
	}

	void test_event_connection()
	{
		int calls = 0;
		{
			auto connection = mngl::event_manager::on<mngl::window_resized>(
			    [&](const mngl::window_resized& event) { calls += event.size.x; });
			mngl::windowResized event{};
			event.window_id = 12;
			event.size = {3, 4};
			mngl::event_manager::emit(event);
			require(calls == 3 && connection.connected(), "modern/legacy event aliases diverged");
		}
		mngl::event_manager::emit(mngl::window_resized{});
		require(calls == 3, "destroyed event connection remained subscribed");
	}

	void test_hit_test_world_bounds()
	{
		mngl::detail::hit_test_frame frame;
		frame.target_size = {100, 100};
		frame.view_projection = glm::mat4(1.0f);
		frame.view_projection[0][0] = 0.02f;
		frame.view_projection[1][1] = 0.02f;
		frame.view_projection[3][0] = -1.0f;
		frame.view_projection[3][1] = -1.0f;

		mngl::detail::hit_test_item item;
		item.draw_index = 7;
		item.destination = {{0.0f, 0.0f}, {10.0f, 10.0f}};
		item.world_to_local = glm::mat4(1.0f);
		item.world_to_local[0][0] = 0.1f;
		item.world_to_local[1][1] = 0.1f;
		item.world_to_local[3][0] = -4.0f;
		item.world_to_local[3][1] = -4.0f;
		item.inverse_valid = true;
		glm::mat4 local_to_world(1.0f);
		local_to_world[0][0] = 10.0f;
		local_to_world[1][1] = 10.0f;
		local_to_world[3][0] = 40.0f;
		local_to_world[3][1] = 40.0f;
		const auto world_bounds =
		    mngl::detail::transformed_bounds(local_to_world, {{0.0f, 0.0f}, {1.0f, 1.0f}});
		require(world_bounds && world_bounds->equals({40.0f, 40.0f, 10.0f, 10.0f}),
		        "transformed hit-test bounds did not include the full local quad");
		item.world_bounds = *world_bounds;
		item.world_bounds_valid = true;
		frame.items.push_back(std::move(item));

		const auto hit = mngl::detail::hit_test_frame_at_pixel(frame, {45.0f, 45.0f});
		require(hit && hit->path.size() == 1 && hit->path.front().draw_index == 7,
		        "hit-test broad phase rejected an item inside its world bounds");
		require(!mngl::detail::hit_test_frame_at_pixel(frame, {5.0f, 5.0f}),
		        "hit-test broad phase accepted a point outside every item bound");
	}

	void test_surface_invalid_update_time()
	{
		temporary_directory temporary;
		const auto animation_path = temporary.path / "two-frame.gif";
		std::array<SDL_Surface*, 2> frames{
		    SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32),
		    SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGBA32)};
		require(frames[0] != nullptr && frames[1] != nullptr,
		        "could not create the surface update test frames");
		std::memset(frames[0]->pixels, 0x00, static_cast<std::size_t>(frames[0]->pitch));
		std::memset(frames[1]->pixels, 0xff, static_cast<std::size_t>(frames[1]->pitch));
		std::array<int, 2> delays{100, 100};
		IMG_Animation animation{.w = 1, .h = 1, .count = 2, .frames = frames.data(), .delays = delays.data()};
		const bool saved = IMG_SaveAnimation(&animation, animation_path.string().c_str());
		SDL_DestroySurface(frames[0]);
		SDL_DestroySurface(frames[1]);
		require(saved, "could not write the surface update test animation");

		mngl::surface image(animation_path.string());
		require(image.animated(), "surface update test file did not load as an animation");
		SDL_Surface* initial_frame = image.raw();
		for (const double invalid_time : {-0.1, std::numeric_limits<double>::infinity(),
		                                  std::numeric_limits<double>::quiet_NaN(),
		                                  std::numeric_limits<double>::max(), 5'000.0})
		{
			bool rejected = false;
			try
			{
				image.update(invalid_time);
			}
			catch (const std::invalid_argument&)
			{
				rejected = true;
			}
			require(rejected, "surface accepted an invalid update time");
			require(image.raw() == initial_frame, "invalid surface update time changed the current frame");
		}
		image.update(0.1);
		require(image.raw() != initial_frame, "valid surface update did not advance after rejected input");
	}

	void test_concurrent_subscriptions()
	{
		std::atomic_int permanent_calls = 0;
		std::atomic_int transient_calls = 0;
		std::atomic_bool start = false;
		auto permanent =
		    mngl::event_manager::on<mngl::window_resized>([&](const auto&) { ++permanent_calls; });
		std::jthread subscriber(
		    [&]
		    {
			    while (!start.load(std::memory_order_acquire))
				    std::this_thread::yield();
			    for (int index = 0; index < 500; ++index)
			    {
				    auto temporary = mngl::event_manager::on<mngl::window_resized>([&](const auto&)
					                                                               { ++transient_calls; });
				    std::this_thread::yield();
			    }
		    });
		start.store(true, std::memory_order_release);
		for (int index = 0; index < 1000; ++index)
			mngl::event_manager::emit(mngl::window_resized{});
		subscriber.join();
		require(permanent_calls == 1000, "Subscription churn dropped events from a permanent listener");
		const auto before = transient_calls.load();
		mngl::event_manager::emit(mngl::window_resized{});
		require(transient_calls == before, "Destroyed concurrent subscription still received events");
	}

	void test_sdl_event_delivery()
	{
		std::atomic_int calls = 0;
		std::thread::id callback_thread;
		auto connection = mngl::event_manager::on<mngl::window_resized>(
		    [&](const mngl::window_resized&)
		    {
			    callback_thread = std::this_thread::get_id();
			    ++calls;
		    });

		SDL_Event event{};
		event.type = SDL_EVENT_WINDOW_RESIZED;
		event.window.windowID = 123;
		event.window.data1 = 640;
		event.window.data2 = 480;
		std::atomic_bool pushed = false;
		std::thread producer([event, &pushed]() mutable { pushed = SDL_PushEvent(&event); });
		producer.join();

		require(pushed.load(), "SDL rejected a window resize event from a worker thread");
		require(calls.load() == 0, "SDL event callback ran on the event posting thread");
		std::atomic_int late_calls = 0;
		auto late_connection = mngl::event_manager::on<mngl::window_resized>(
		    [&late_calls](const mngl::window_resized&) { ++late_calls; });
		const auto pump_thread = std::this_thread::get_id();
		mngl::poll_events();
		require(calls.load() == 1, "SDL event was not delivered during event pumping");
		require(late_calls.load() == 0,
		        "A listener added after an SDL event was queued must not receive that earlier event");
		require(callback_thread == pump_thread, "SDL event callback did not run on the pumping thread");
		for (int index = 0; index < 32; ++index)
		{
			event.window.data1 = 640 + index;
			require(SDL_PushEvent(&event), "SDL rejected a repeated window resize event");
		}
		mngl::poll_events();
		require(calls.load() == 33, "repeated SDL events were lost while reusing callback storage");
		require(late_calls.load() == 32, "A listener added between queued batches missed later SDL events");

		auto throwing_connection = mngl::event_manager::on<mngl::window_resized>(
		    [](const mngl::window_resized&) { throw std::runtime_error("event callback failure"); });
		event.window.data1 = 800;
		require(SDL_PushEvent(&event), "SDL rejected a window resize event for the exception test");
		bool propagated = false;
		try
		{
			mngl::poll_events();
		}
		catch (const std::runtime_error& error)
		{
			propagated = std::string_view(error.what()) == "event callback failure";
		}
		require(propagated, "SDL event callback exception did not propagate through the C++ event pump");
	}

	void test_value_contracts()
	{
		const mngl::color zero(0.0f, 0.0f, 0.0f, 1.0f);
		const mngl::color negative_zero(-0.0f, 0.0f, 0.0f, 1.0f);
		require(zero == negative_zero &&
		            std::hash<mngl::color>{}(zero) == std::hash<mngl::color>{}(negative_zero),
		        "equal colors must hash equally");
		mngl::window::flags flags;
		flags.resizable = true;
		require(mngl::window::flags(static_cast<SDL_WindowFlags>(flags)).resizable,
		        "resizable flag did not round-trip");
	}

	void test_color_encoding()
	{
		const mngl::color value(0x12345678u);
		require(value.rgb_uint() == 0x123456u, "RGB encoding included alpha or omitted red");
		require(mngl::color{"#aBcDeF"}.rgba_uint() == 0xABCDEFFFu,
		        "HEX parser lost a valid mixed-case RGB color");
		require(mngl::color{"#12345678"} == value, "HEX parser lost a valid RGBA color");
		for (const std::string invalid : {"#12345g", "#12 345", "#-00001", "#+00001", "#0x1234"})
		{
			bool rejected = false;
			try
			{
				(void)mngl::color(invalid);
			}
			catch (const std::invalid_argument&)
			{
				rejected = true;
			}
			require(rejected, "HEX parser accepted characters outside the HEX color grammar");
		}
	}

	void test_rectangle_contracts()
	{
		const mngl::rectangle area{0.0f, 0.0f, 4.0f, 4.0f};
		const SDL_FRect sdl_area = area;
		const std::array cases{
		    mngl::rectangle{4.0f, 1.0f, 2.0f, 2.0f}, mngl::rectangle{4.0f, 4.0f, 0.0f, 0.0f},
		    mngl::rectangle{1.0f, 1.0f, 0.0f, 2.0f}, mngl::rectangle{2.0f, 2.0f, -1.0f, 1.0f},
		    mngl::rectangle{10.0f, 10.0f, 1.0f, 1.0f}};
		for (const auto& value : cases)
		{
			const SDL_FRect sdl_value = value;
			const bool expected = SDL_HasRectIntersectionFloat(&sdl_area, &sdl_value);
			require(area.intersects(value) == expected && value.intersects(area) == expected,
			        "Rectangle intersection disagreed with SDL float rectangle semantics");
			SDL_FRect overlap{};
			if (SDL_GetRectIntersectionFloat(&sdl_area, &sdl_value, &overlap))
			{
				require(area.intersection(value).equals(overlap),
				        "Rectangle intersection bounds differ from SDL");
			}
			else
			{
				require(area.intersection(value).empty(), "Disjoint rectangle intersection was not empty");
			}
		}

		const mngl::rectangle empty{2.0f, 2.0f, -1.0f, -1.0f};
		const mngl::rectangle elsewhere{10.0f, 10.0f, 4.0f, 4.0f};
		require(empty.union_with(elsewhere) == elsewhere && elsewhere.union_with(empty) == elsewhere,
		        "An empty rectangle expanded a union");
		require(!empty.intersects_line({1.0f, 1.0f}, {3.0f, 3.0f}),
		        "An empty rectangle reported a line intersection");

		const std::array<glm::vec2, 3> points{{{-1.0f, -1.0f}, {2.0f, 3.0f}, {8.0f, 9.0f}}};
		const auto bounds = enclosing_points(std::span<const glm::vec2>(points), area);
		require(bounds == mngl::rectangle{2.0f, 3.0f, 0.0f, 0.0f},
		        "Clipped point bounds included points outside the clip");
		const std::array<glm::vec2, 2> outside{{{-1.0f, -1.0f}, {8.0f, 9.0f}}};
		require(enclosing_points(std::span<const glm::vec2>(outside), area).empty(),
		        "Point bounds were nonempty without any point inside the clip");
	}

	void test_files()
	{
		temporary_directory temporary;
		{
			mngl::storage::file_storage store(temporary.path);
			require(store.ready(), "local storage is not ready");
			store.write_text_file("данные.txt", "C++23: привет");
			require(store.read_text_file("данные.txt") == "C++23: привет", "storage lost UTF-8 bytes");
			require(store.enumerate().size() == 1, "storage enumeration mismatch");
			store.close();
			require(!store, "closed storage kept its handle");
		}
		mngl::async_io::queue queue;
		const auto async_path = temporary.path / std::filesystem::path(u8"асинхронный.txt");
		auto write = queue.write_text_file(async_path, "asynchronous value");
		const auto wait_for = [&](const mngl::async_io::request& request)
		{
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
			while (!request.ready())
			{
				require(std::chrono::steady_clock::now() < deadline, "async request timed out");
				(void)queue.wait(std::chrono::milliseconds(20));
			}
			require(request.result().status == mngl::async_io::request_status::completed,
			        "async request failed");
		};
		wait_for(write);
		auto read = queue.read_file(async_path);
		auto shared = read;
		auto owner = std::move(read);
		require(!read && read.ready() && read.result().status == mngl::async_io::request_status::failed,
		        "moved-from request did not report an empty failure");
		mngl::async_io::queue other_queue;
		const auto wrong_queue_result = other_queue.wait(owner);
		require(!wrong_queue_result &&
		            wrong_queue_result.error().code == mngl::async_io::request_error_code::wrong_queue,
		        "waiting on a request from another queue was accepted");
		const auto outcome = queue.wait(owner);
		require(outcome && outcome->info.status == mngl::async_io::request_status::completed &&
		            outcome->info.bytes_transferred == outcome->data.size() &&
		            std::string(reinterpret_cast<const char*>(outcome->data.data()), outcome->data.size()) ==
		                "asynchronous value",
		        "async request outcome lost its data or metadata");
		require(shared.ready(), "shared async request did not observe completion");
		bool threw = false;
		try
		{
			(void)shared.take_data();
		}
		catch (const std::logic_error&)
		{
			threw = true;
		}
		require(threw, "async buffer could be consumed twice");
	}

	void test_dialog_arguments()
	{
		const auto rejects_argument = [](auto&& action)
		{
			try
			{
				action();
			}
			catch (const std::invalid_argument&)
			{
				return true;
			}
			return false;
		};
		mngl::dialog::file_dialog_options options;
		options.filters = {{std::string("Text\0Files", 10), "txt"}};
		require(rejects_argument([&] { (void)mngl::dialog::open_file(options); }),
		        "dialog accepted a truncated filter name");
		options.filters = {{"Text", std::string("txt\0png", 7)}};
		require(rejects_argument([&] { (void)mngl::dialog::open_files(options); }),
		        "dialog accepted a truncated filter pattern");
		options.filters.clear();
		const std::filesystem::path invalid_path(std::u8string(u8"bad\0path", 8));
		options.default_location = invalid_path;
		require(rejects_argument([&] { (void)mngl::dialog::save_file(options); }),
		        "dialog accepted a truncated default file location");
		mngl::dialog::folder_dialog_options folder_options;
		folder_options.default_location = invalid_path;
		require(rejects_argument([&] { (void)mngl::dialog::open_folder(folder_options); }),
		        "dialog accepted a truncated default folder location");
	}

	void test_process_input(const std::string& executable)
	{
		mngl::process::launch_options options;
		options.standard_input = mngl::process::stdio_route::pipe;
		options.standard_error = mngl::process::stdio_route::discard;
		auto child = mngl::process::launch({executable, "--child-eof"}, options);
		const std::string_view input = "pipe payload";
		std::size_t written = 0;
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		while (written < input.size())
		{
			require(std::chrono::steady_clock::now() < deadline, "process input write timed out");
			written += child.write_stdin(input.substr(written));
			if (written < input.size())
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		child.close_stdin();
		child.close_stdin();
		bool rejected_write = false;
		try
		{
			(void)child.write_stdin("after EOF");
		}
		catch (const std::logic_error&)
		{
			rejected_write = true;
		}
		require(rejected_write, "process input accepted a write after closing stdin");
		std::string output;
		while (true)
		{
			const auto chunk = child.read_stdout();
			require(chunk.state != mngl::process::stream_state::failed, "failed to read process output");
			if (!chunk.data.empty())
				output.append(reinterpret_cast<const char*>(chunk.data.data()), chunk.data.size());
			if (chunk.state == mngl::process::stream_state::end_of_stream)
				break;
			if (std::chrono::steady_clock::now() >= deadline)
			{
				(void)child.terminate();
				throw std::runtime_error("process did not finish after stdin EOF");
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		require(child.wait() == 9 && output == "eof:pipe payload",
		        "process lost input or did not observe stdin EOF");
	}
} // namespace

int main(int argc, char** argv)
{
	if (argc == 2 && std::string_view(argv[1]) == "--child")
	{
		std::cout << "child-result";
		return 7;
	}
	if (argc == 2 && (std::string_view(argv[1]) == "--hit-test" ||
	                  std::string_view(argv[1]) == "--event-test" ||
	                  std::string_view(argv[1]) == "--surface-test"))
	{
		try
		{
			mngl::runtime::ensure();
			if (std::string_view(argv[1]) == "--hit-test")
				test_hit_test_world_bounds();
			else if (std::string_view(argv[1]) == "--surface-test")
				test_surface_invalid_update_time();
			else
				test_sdl_event_delivery();
			return 0;
		}
		catch (const std::exception& error)
		{
			std::cerr << error.what() << '\n';
			return 1;
		}
	}
	if (argc == 2 && std::string_view(argv[1]) == "--child-eof")
	{
		const std::string input{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
		std::cout << "eof:" << input;
		return 9;
	}
	try
	{
		mngl::runtime::ensure();
		test_hit_test_world_bounds();
		test_surface_invalid_update_time();
		test_errors_and_paths();
		test_event_connection();
		test_concurrent_subscriptions();
		test_sdl_event_delivery();
		test_value_contracts();
		test_color_encoding();
		test_rectangle_contracts();
		test_files();
		test_dialog_arguments();
		test_process_input(std::filesystem::absolute(argv[0]).string());
		auto result =
		    mngl::process::child_process::run({std::filesystem::absolute(argv[0]).string(), "--child"});
		require(result.exit_code == 7 && result.output == "child-result",
		        "process run lost output or exit code");
		bool threw = false;
		try
		{
			(void)mngl::process::launch({});
		}
		catch (const std::invalid_argument&)
		{
			threw = true;
		}
		require(threw, "empty process arguments were accepted");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
