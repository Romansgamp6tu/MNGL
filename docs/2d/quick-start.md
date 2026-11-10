# Быстрый старт

Минимальный пример: загрузить изображение сразу в текстуру и рисовать её в окне.

```cpp
#include <MNGL/mngl.hpp>

int main()
{
	mngl::render_window window("MNGL 2D", {800, 600});
	mngl::texture image("assets/image.png");
	mngl::sprite image_sprite{image, mngl::rectangle{0.1f, 0.1f, 0.5f, 0.5f}};

	while (mngl::input::next_frame())
	{
		auto frame = window.frame();
		frame.draw(image_sprite);
		frame.display();
	}
}
```

`mngl::texture(path)` загружает статичное изображение через SDL_image и создаётся с возможностью семплирования и рендеринга в неё. Используйте `mngl::animated_texture`, чтобы автоматически обновлять GPU-текстуру для анимации, или `mngl::surface`, если нужно работать с SDL-кадрами напрямую. Прямоугольник задаётся как `{x, y, width, height}` в логических координатах; начало координат находится в левом нижнем углу. См. [координаты и view](coordinates-and-view.md).

Если изображение нужно нарисовать в его естественном размере у начала координат, используйте `mngl::sprite image_sprite{ image };`. Размер берётся из `image.coordinate_size()`; для явного положения и размера остаётся конструктор с `rectangle`.

`window.frame()` начинает и очищает кадр; `frame.draw()` принимает тот же drawable, что и `window.draw()`, а `frame.display()` явно завершает и отправляет кадр. Если scope уничтожен без `display()` — например, при выходе из-за исключения, — кадр отменяется и окно снова готово к следующему кадру.

Спрайт не владеет исходной текстурой: `image` должна оставаться живой до `frame.display()`.
