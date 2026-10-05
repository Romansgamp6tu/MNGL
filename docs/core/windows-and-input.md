# Окна и ввод

## Окно и дисплеи

`mngl::window` — RAII-обёртка окна SDL. Конструктор принимает заголовок, размер и необязательные флаги:

```cpp
mngl::window::flags flags{};
flags.resizeable = true;

mngl::window window("My app", { 1280, 720 }, flags);
```

Флаги окна задаются полями `window::flags`; среди них fullscreen, borderless, high-pixel-density, modal и platform graphics backend. Большинство свойств окна читаются и меняются через пары методов вроде `size()` / `size(new_size)` и `title()` / `title(new_title)`. Для 2D-отрисовки используйте `mngl::render_window`, который расширяет окно рендерингом.

`window::size()` возвращает размер окна в оконных координатах, а `pixel_size()` — фактический размер drawable-области в пикселях. Они могут различаться на дисплеях с высокой плотностью пикселей.

`mngl::display` позволяет получить список дисплеев (`displays()`), основной дисплей (`primary()`), границы, рабочую область, масштаб и видеорежимы. Окно предоставляет удобные методы `attached_display()` и `mode()`.

## Основной цикл и состояние ввода

Обычно приложение вызывает `Input::continue_work()` один раз за кадр:

```cpp
#include "input.hpp"

while (mngl::Input::continue_work())
{
    if (mngl::Input::is_key_pressed(mngl::scancode::a))
    {
        // Обработать нажатие в текущем кадре.
    }

    const double dt = mngl::Input::get_dt();
    // Обновить состояние приложения и отрисовать кадр.
}
```

`continue_work()` обрабатывает события, обновляет состояние ввода и вызывает `update(dt)` у зарегистрированных `mngl::Iupdatable`. Он возвращает `false` после запроса закрытия приложения или когда `Input::should_work` сброшен. `get_dt()` возвращает время между кадрами в секундах.

Для клавиатуры доступны `is_key_pressed`, `is_key_holded` и `is_key_released`; код клавиши задаётся `mngl::scancode`. Аналогичные методы есть для кнопок мыши. Положение указателя, его смещение и прокрутка доступны через `get_mouse_position()`, `get_mouse_delta()` и `get_mouse_scroll_delta()`.

Ограничение частоты кадров по умолчанию равно 60 FPS. Вызов `set_max_fps(0)` отключает ожидание между кадрами.
