#include "input.hpp"
#include "render_window.hpp"

int main()
{
	mngl::render_window window("MNGL 2D rectangle", { 800, 600 });
	const mngl::rectangle bounds{ 0.4f, 0.25f, 0.5f, 0.5f };
	const mngl::color fill{ 0.9f, 0.25f, 0.15f, 1.0f };

	while (mngl::Input::continue_work())
	{
		window.clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f });
		window.draw(bounds, fill);
		window.display();
	}
}
