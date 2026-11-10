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

При отсутствии пересечения `intersection()` возвращает прямоугольник с отрицательным размером. `rectangle` также преобразуется в SDL `SDL_Rect` и `SDL_FRect`.

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

`intersects(other)` проверяет пересечение отрезков, а `intersection_point(other)` возвращает `std::optional<glm::vec2>` — точку пересечения или пустое значение. Функция `lines_intersect(...)` предоставляет проверку без создания объектов `line`.
