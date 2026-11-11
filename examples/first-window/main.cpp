#include <MNGL/input.hpp>
#include <MNGL/render_window.hpp>

#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::render_window window("My first MNGL scene", {800, 600});
		mngl::rectangle player{0.2f, 0.2f, 0.25f, 0.25f};

		while (mngl::input::next_frame())
		{
			for (const auto& keyboard : mngl::input::keyboards())
			{
				if (keyboard.pressed(mngl::scancode::escape))
				{
					mngl::input::request_quit();
				}
			}

			auto frame = window.frame(mngl::color{0.07f, 0.09f, 0.14f, 1.0f});
			frame.draw(mngl::two_d::rectangle_shape{
			    player, mngl::color{0.2f, 0.75f, 0.95f, 1.0f}});
			frame.display();
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << "MNGL: " << error.what() << '\n';
		return 1;
	}
}
