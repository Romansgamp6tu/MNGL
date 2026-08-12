#include "window.hpp"
#include "input.hpp"

int main()
{
	mngl::window win("hello, world!", {800, 600});
	
	while (mngl::Input::continue_work())
	{
		if (mngl::Input::is_key_released(mngl::scancode::a))
		{
			win.borderless(false);
		}
		if (mngl::Input::is_key_released(mngl::scancode::b))
		{
			win.borderless(true);
		}
	}
}