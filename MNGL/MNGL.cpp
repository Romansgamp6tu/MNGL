#include "input.hpp"
#include "render_window.hpp"

int main()
{
	mngl::render_window window("MNGL 2D hit testing", { 800, 600 });
	window.enable_hit_testing();
	auto panel = window.create_texture({ 400, 300 });
	panel.enable_hit_testing();
	panel.set_view(mngl::view{ mngl::rectangle{ 0.0f, 0.0f, 1.2f, 0.9f } });

	const mngl::rectangle bounds{ 0.2f, 0.2f, 0.8f, 0.5f };
	const mngl::rectangle panel_bounds{ 0.26666668f, 0.2f, 0.8f, 0.6f };
	mngl::sprite panel_sprite(panel, panel_bounds);
	mngl::two_d::transform panel_transform;
	panel_transform.rotate(0.12f, bounds.pos + bounds.size * 0.5f);
	mngl::two_d::rounded_rectangle button;
	button.bounds = bounds;
	button.radius = 0.08f;
	button.transform = panel_transform;
	bool button_clicked = false;
	button.on_click([&](const mngl::hit_test_result&, const mngl::hit_test_path_entry&)
	{
		button_clicked = true;
	});
	bool has_completed_frame = false;

	while (mngl::Input::continue_work())
	{
		if (has_completed_frame &&
			mngl::Input::is_mouse_button_pressed(mngl::mouse_button::left))
		{
			(void)window.dispatch_click(mngl::Input::get_mouse_position());
		}

		mngl::color fill{ 0.9f, 0.25f, 0.15f, 1.0f };
		if (has_completed_frame)
		{
			if (const auto hit = window.hit_test(mngl::Input::get_mouse_position());
				hit && hit->path.size() > 1)
			{
				const auto local = hit->path.back().local_coords;
				fill = {
					local.x / bounds.size.x,
					local.y / bounds.size.y,
					0.15f,
					1.0f
				};
			}
		}
		if (button_clicked)
		{
			fill = { 0.2f, 0.75f, 0.3f, 1.0f };
		}

		panel.clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f });
		panel.draw([&](mngl::two_d::Frame2D& frame)
		{
			button.tint = fill;
			frame.draw(button);
		});
		panel.display();

		window.clear(mngl::color{ 0.08f, 0.09f, 0.12f, 1.0f });
		window.draw(panel_sprite);
		window.display();
		has_completed_frame = true;
	}
}
