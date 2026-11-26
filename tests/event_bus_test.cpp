#define APP_NAME "MNGL event bus test"
#define APP_VERSION 1
#define APP_FLAGS (SDL_INIT_VIDEO | SDL_INIT_EVENTS)

#include <MNGL/event.hpp>
#include <MNGL/runtime.hpp>

#include <iostream>
#include <stdexcept>

namespace
{
	struct bus_test_event : mngl::event_base
	{
		int value{};
	};

	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	void test_buses_are_isolated()
	{
		mngl::event_bus gameplay_events;
		mngl::event_bus interface_events;
		int gameplay_calls = 0;
		int interface_calls = 0;
		int global_calls = 0;
		auto gameplay_connection = gameplay_events.on<bus_test_event>(
		    [&](const bus_test_event& event) { gameplay_calls += event.value; });
		auto interface_connection = interface_events.on<bus_test_event>(
		    [&](const bus_test_event& event) { interface_calls += event.value; });
		auto global_connection = mngl::event_manager::on<bus_test_event>(
		    [&](const bus_test_event& event) { global_calls += event.value; });

		bus_test_event event{};
		event.value = 3;
		gameplay_events.emit(event);
		require(gameplay_calls == 3 && interface_calls == 0 && global_calls == 0,
		        "An event reached a bus other than the one it was emitted on");

		interface_events.emit(event);
		require(gameplay_calls == 3 && interface_calls == 3 && global_calls == 0,
		        "An event on the second bus reached another bus");

		mngl::event_manager::emit(event);
		require(gameplay_calls == 3 && interface_calls == 3 && global_calls == 3,
		        "A globally emitted event reached an isolated bus");
	}

	void test_disconnect_clear_and_reuse()
	{
		mngl::event_bus events;
		int disconnected_calls = 0;
		int retained_calls = 0;
		auto disconnected = events.on<bus_test_event>(
		    [&](const bus_test_event&) { ++disconnected_calls; });
		auto retained = events.on<bus_test_event>([&](const bus_test_event&) { ++retained_calls; });

		events.emit(bus_test_event{});
		disconnected.disconnect();
		events.emit(bus_test_event{});
		require(!disconnected.connected() && disconnected_calls == 1 && retained_calls == 2,
		        "Disconnecting one subscription affected the wrong listeners");

		events.clear();
		require(!retained.connected(), "clear() left a subscription connected");
		events.emit(bus_test_event{});
		require(retained_calls == 2, "A callback ran after clear()");

		auto after_clear = events.on<bus_test_event>([&](const bus_test_event&) { ++retained_calls; });
		events.emit(bus_test_event{});
		require(after_clear.connected() && retained_calls == 3, "The bus could not be reused after clear()");
	}

	void test_destruction_disconnects_connections()
	{
		mngl::event_connection connection;
		{
			mngl::event_bus events;
			connection = events.on<bus_test_event>([](const bus_test_event&) {});
		}
		require(!connection.connected(), "Destroying a bus left its connection active");
	}
} // namespace

int main()
{
	try
	{
		mngl::runtime::ensure();
		test_buses_are_isolated();
		test_disconnect_clear_and_reuse();
		test_destruction_disconnects_connections();
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
