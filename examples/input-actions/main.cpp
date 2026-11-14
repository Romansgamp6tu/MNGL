#include <MNGL/action_map.hpp>
#include <MNGL/input.hpp>
#include <MNGL/runtime.hpp>
#include <MNGL/window.hpp>

#include <exception>
#include <iostream>
#include <string>

int main()
{
	try
	{
		mngl::runtime::ensure();
		mngl::window window("Actions", {800, 600});

		mngl::action_map controls;
		controls.bind_key("move_left", mngl::scancode::a);
		controls.bind_key("move_right", mngl::scancode::d);
		controls.bind_key("jump", mngl::scancode::space);
		controls.bind_key("pause", mngl::scancode::escape);

		mngl::input_context global_controls(controls, window.id());
		mngl::input_context gameplay(controls, window.id());
		double player_x = 0.0;
		bool paused = false;

		while (mngl::input::next_frame())
		{
			if (global_controls.pressed("pause"))
			{
				paused = !paused;
				gameplay.set_enabled(!paused);
			}

			const double direction = static_cast<double>(gameplay.down("move_right")) -
			                         static_cast<double>(gameplay.down("move_left"));
			player_x += direction * 100.0 * mngl::input::delta_time();
			if (gameplay.pressed("jump"))
				std::cout << "Jump\n";

			window.title(std::string(paused ? "Paused, x=" : "Playing, x=") +
			             std::to_string(player_x));
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
