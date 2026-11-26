# Геометрия

## Прямоугольник

`mngl::rectangle` хранит позицию и размер. Конструктор с четырьмя числами принимает `{x, y, width, height}`:

```cpp
mngl::rectangle rect{10.0f, 20.0f, 100.0f, 50.0f};
```

Поля доступны и как `pos` / `size`, и как `x`, `y`, `w`, `h`. Основные операции:

- `contains(point)`, `intersects(other)` и `intersects_line(...)`;
- `intersection(other)` и `union_with(other)`;
- `center()`, `area()`, `perimeter()`, `inflate(amount)` и `clamp(point)`;
- `local(point)` и `global(point)` для перехода между локальными и глобальными координатами прямоугольника.

Вот типичный шаг проверки столкновения и ограничения точки внутри области.
Фрагмент предполагает, что код находится внутри функции приложения:

```cpp
#include <MNGL/rectangle.hpp>

#include <glm/vec2.hpp>

const mngl::rectangle player{20.0f, 20.0f, 24.0f, 32.0f};
const mngl::rectangle pickup{38.0f, 28.0f, 12.0f, 12.0f};

if (player.intersects(pickup))
{
	const float overlap_area = player.intersection_area(pickup);
	const mngl::rectangle overlap = player.intersection(pickup);
	// overlap_area == 72; overlap описывает общую область.
}

const mngl::rectangle room{0.0f, 0.0f, 640.0f, 360.0f};
const glm::vec2 requested_position{-5.0f, 365.0f};
const glm::vec2 clamped_position = room.clamp(requested_position);
// clamped_position == {0, 360}.
```

`contains()` и `clamp()` работают с точкой, а не с прямоугольником объекта.
Чтобы целиком удерживать игрока в комнате, ограничивайте позицию его левого
верхнего угла на диапазон от `room.pos` до `room.pos + room.size - player.size`.
`rectangle::clamp()` напрямую не учитывает размер игрока.

`local(point)` вычитает начало прямоугольника, а `global(point)` прибавляет его.
Следующий фрагмент также находится внутри функции приложения:

```cpp
const glm::vec2 world_point{125.0f, 78.0f};
const mngl::rectangle panel{100.0f, 50.0f, 300.0f, 200.0f};
const glm::vec2 panel_point = panel.local(world_point); // {25, 28}
const glm::vec2 same_world_point = panel.global(panel_point);
```

При отсутствии пересечения `intersection()` возвращает прямоугольник с
отрицательным размером. Проверяйте его через `empty()`, а для площади общего
участка используйте `intersection_area()` — она возвращает ноль, когда
пересечения нет. `rectangle` преобразуется в SDL `SDL_Rect` и `SDL_FRect`;
при преобразовании в целочисленный `SDL_Rect` дробные координаты усекаются.

Как у SDL floating-point rectangles, отрицательная ширина или высота означает
пустую область, а нулевой размер может задавать отрезок или точку. Границы
включены: касание краёв считается пересечением и даёт нулевую ширину или высоту.
Пустая область не пересекается с отрезком и не расширяет `union_with()`.

`enclosing_points(points, clip)` строит границы только тех точек, которые находятся
внутри `clip`. Точки за пределами clip игнорируются; если подходящих точек нет,
результат пустой. Чтобы пересечь границы всех точек с clip, сначала вычислите их
общий bounding box, затем вызовите `intersection(clip)`.

## Отрезок

`mngl::line` задаётся начальной и конечной точками:

```cpp
mngl::line segment{{0.0f, 0.0f}, {1.0f, 1.0f}};
const float length = segment.length();
```

`intersects(other)` проверяет пересечение именно отрезков, а не бесконечных
прямых. `intersection_point(other)` возвращает `std::optional<glm::vec2>` —
общую точку или `std::nullopt`, если отрезки не пересекаются. Для совпадающих
или накладывающихся коллинеарных отрезков возвращается первая общая точка вдоль
первого отрезка. Функция `lines_intersect(...)` предоставляет проверку без
создания объектов `line`.

```cpp
// Фрагмент внутри функции приложения.
const mngl::line cursor_ray{{0.0f, 20.0f}, {100.0f, 20.0f}};
const mngl::line wall{{40.0f, 0.0f}, {40.0f, 50.0f}};

if (const auto hit = cursor_ray.intersection_point(wall))
{
	// hit содержит {40, 20}.
}
```

Не вызывайте `direction()` на отрезке нулевой длины: нормализовать нулевой
вектор нельзя. Если входная геометрия может содержать такой отрезок, сначала
проверьте `length()`.

