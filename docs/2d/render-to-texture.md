# Рендер сцены в текстуру

Render-target текстура позволяет сначала собрать изображение отдельно, затем использовать его как обычный спрайт. Так делают миникарту, превью редактора, фон меню или сцену с фиксированным внутренним разрешением.

Процесс состоит из двух последовательных кадров:

1. Нарисовать сцену в текстуру и завершить этот кадр.
2. Нарисовать текстуру в окно и завершить кадр окна.

## Полный пример без внешних ресурсов

Внутренняя сцена имеет размер `640 × 360` пикселей и камеру `16 × 9` единиц. Окно показывает её в большем разрешении:

```cpp
#include <MNGL/input.hpp>
#include <MNGL/render_window.hpp>
#include <MNGL/texture.hpp>
#include <MNGL/view.hpp>

#include <cmath>
#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::render_window window("Offscreen scene", {1280, 720});
		const mngl::view camera{mngl::rectangle{0.0f, 0.0f, 16.0f, 9.0f}};
		window.set_view(camera);

		mngl::texture scene = window.create_texture({640, 360});
		scene.set_view(camera);
		mngl::sprite preview{scene, mngl::rectangle{0.0f, 0.0f, 16.0f, 9.0f}};
		double time = 0.0;

		while (mngl::input::next_frame())
		{
			time += mngl::input::delta_time();
			const float x = 7.0f + 4.0f * static_cast<float>(std::sin(time));

			auto scene_frame = scene.frame(mngl::color{0.04f, 0.08f, 0.12f, 1.0f});
			scene_frame.draw(mngl::two_d::rectangle_shape{
			    {x, 3.5f, 2.0f, 2.0f}, mngl::color{1.0f, 0.55f, 0.15f, 1.0f}});
			scene_frame.display();

			auto window_frame = window.frame(mngl::color{0.01f, 0.01f, 0.01f, 1.0f});
			window_frame.draw(preview);
			window_frame.display();
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << "MNGL: " << error.what() << '\n';
		return 1;
	}
}
```

Собирайте его с тем же target `MNGL::MNGL`, что и [первое приложение](quick-start.md). Квадрат движется внутри текстуры, а окно выводит уже готовое изображение.

`window.create_texture()` создаёт цель на GPU device окна и по умолчанию выбирает подходящий формат. У обычного `mngl::texture(width, height)` также есть флаги `sampler | color_target`: такую текстуру можно и рисовать, и использовать как источник. При явном указании usage сохраните `color_target` для цели и `sampler` для последующего вывода.

## Разрешение и размер в мире

`640 × 360` определяет количество пикселей внутренней сцены. Камера `16 × 9` определяет её единицы мира. Прямоугольник `{x, 3.5f, 2.0f, 2.0f}` имеет сторону `2` единицы независимо от разрешения текстуры.

При изменении размера окна текстура сохраняет свои `640 × 360`. Это удобно для постоянного внутреннего разрешения. `preview.destination` определяет, насколько большой она будет в мире окна. При несовпадении пропорций явный destination может растянуть изображение; в примере view окна сохраняет область `16 × 9` и расширяет видимый мир по одной оси.

Окно и текстура хранят свои view отдельно. Изменение `window.set_view(...)` не меняет камеру `scene`. Подробнее — [координаты и view](coordinates-and-view.md).

## Цель размером с framebuffer

Если внутреннее разрешение должно следовать за окном, пересоздавайте цель **между кадрами**. В полном примере этот фрагмент можно поставить в начало цикла, до `scene.frame()`:

```cpp
const auto new_size = window.pixel_size();
if (new_size.x > 0 && new_size.y > 0 && scene.size() != new_size)
{
	scene = window.create_texture(new_size);
	scene.set_view(camera);
}
```

Проверка ненулевого размера нужна, например, при сворачивании окна. После пересоздания снова установите view текстуры. `preview` по-прежнему ссылается на тот же C++ объект `scene`, содержимое которого было заменено.

Для offscreen-вывода Dear ImGui workspace у MNGL есть специальная `resizable_texture`; её сценарий описан в [руководстве по окнам и ImGui](../core/windows-and-input.md).

## Рендер через окно в заданную цель

Вместо `scene.frame()` можно начать кадр окна с явной текстурой:

```cpp
auto frame = window.frame(scene, mngl::color{0.0f, 0.0f, 0.0f, 1.0f});
frame.draw(mngl::two_d::rectangle_shape{
    {1.0f, 1.0f, 2.0f, 2.0f}, mngl::color{0.2f, 0.9f, 0.5f, 1.0f}});
frame.display();
```

Это тоже рендеринг в `scene`, а не показ содержимого на экране. Для вывода результата нужен следующий кадр swapchain. В таком кадре используется view текстуры; view окна применяется к его обычной swapchain-цели.

Выбирайте один способ начала конкретного кадра: `scene.frame()` или `window.frame(scene)`. Одновременно использовать одну текстуру как цель двух активных кадров нельзя.

## Время жизни и частые ошибки

- Целевая текстура должна существовать до завершения или отмены своего кадра.
- `preview` заимствует `scene`. Результат должен существовать до отправки кадра окна через `window_frame.display()`.
- `scene.frame()` очищает цель при начале каждого кадра. Для редко меняющегося содержимого используйте [cached_layer](cached-layers.md), который управляет повторным обновлением.
- Замена текстуры и `update_pixels()` во время рендеринга в неё вызывают `std::logic_error`. Сначала завершите активный кадр.
- Для обработки предыдущего результата двумя проходами используйте две текстуры: источник первого прохода не должен одновременно быть его render target.
- Если scope уничтожен без `display()`, активный кадр отменяется. Это помогает при исключении, но такой кадр не появится на экране.

Для независимых сцен в разных offscreen-текстурах и их последующей сборки изучите [параллельную запись](parallel-recording.md). Для более сложных зависимостей ресурсов есть [граф кадра](../gpu/frame-graph.md).
