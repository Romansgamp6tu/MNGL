# Геометрия

## Прямоугольник

`mngl::rectangle` хранит позицию и размер. Конструктор с четырьмя числами принимает `{x, y, width, height}`:

```cpp
mngl::rectangle rect{ 10.0f, 20.0f, 100.0f, 50.0f };
```

Поля доступны и как `pos` / `size`, и как `x`, `y`, `w`, `h`. Основные операции:

- `contains(point)`, `intersects(other)` и `intersectsLine(...)`;
- `intersection(other)` и `unionWith(other)`;
- `center()`, `area()`, `perimeter()`, `inflate(amount)` и `clamp(point)`;
- `local(point)` и `global(point)` для перехода между локальными и глобальными координатами прямоугольника.

При отсутствии пересечения `intersection()` возвращает прямоугольник с отрицательным размером. `rectangle` также преобразуется в SDL `SDL_Rect` и `SDL_FRect`.

## Отрезок

`mngl::line` задаётся начальной и конечной точками:

```cpp
mngl::line segment{{ 0.0f, 0.0f }, { 1.0f, 1.0f }};
const float length = segment.length();
```

`intersects(other)` проверяет пересечение отрезков, а `intersectionPoint(other)` возвращает `std::optional<glm::vec2>` — точку пересечения или пустое значение. Функция `linesIntersect(...)` предоставляет проверку без создания объектов `line`.
