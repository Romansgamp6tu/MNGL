# Быстрый старт

Минимальный пример: загрузить изображение через `mngl::surface`, создать из него текстуру и рисовать её в окне.

```cpp
#include "input.hpp"
#include "render_window.hpp"
#include "surface.hpp"
#include "texture.hpp"

int main()
{
    mngl::render_window window("MNGL 2D", { 800, 600 });
    mngl::surface source_image("assets/image.png");
    mngl::texture image(source_image);

    while (mngl::Input::continue_work())
    {
        window.clear();
        window.draw(image, mngl::rectangle{ 0.1f, 0.1f, 0.5f, 0.5f });
        window.display();
    }
}
```

`mngl::surface` загружает файл через SDL_image, а `mngl::texture` создаётся с возможностью семплирования и рендеринга в неё. Прямоугольник задаётся как `{x, y, width, height}` в логических координатах; начало координат находится в левом нижнем углу. См. [координаты и view](coordinates-and-view.md).

Текстура в `draw(image, ...)` используется без владения: она должна оставаться живой до завершения кадра вызовом `display()`.
