# Произвольная геометрия

`mngl::two_d::vertex_array` рисует набор точек, линий или треугольников. Координаты задаются в единицах текущего 2D-мира; `tint` применяется ко всему объекту.

```cpp
mngl::two_d::vertex_array mesh{mngl::two_d::primitive_type::triangles};
mesh.add_vertex({0.2f, 0.2f})
    .add_vertex({0.8f, 0.2f})
    .add_vertex({0.5f, 0.8f});
mesh.tint = mngl::color{0.2f, 0.7f, 1.0f};

auto frame = window.frame();
frame.draw(mesh);
frame.display();
```

Поддерживаются `points`, `lines`, `line_strip`, `triangles`, `triangle_strip` и `triangle_fan`. Для линий библиотека создаёт полосы заданной толщины; для точек — квадратные маркеры. `line_width` и `point_size` измеряются в координатах мира. У `line_strip` установите `closed = true`, чтобы соединить последнюю точку с первой.

```cpp
mngl::two_d::vertex_array contour{mngl::two_d::primitive_type::line_strip};
contour.add_vertex({0.2f, 0.2f})
    .add_vertex({0.8f, 0.2f})
    .add_vertex({0.7f, 0.7f})
    .add_vertex({0.3f, 0.8f});
contour.closed = true;
contour.line_width = 0.012f;
contour.tint = mngl::color{1.0f, 0.8f, 0.2f};
auto frame = window.frame();
frame.draw(contour);
frame.display();
```

Для залитой фигуры с контуром есть `mngl::two_d::convex_polygon`. Точки должны идти по границе в одном порядке и описывать выпуклую фигуру. Контур опционален: при `outline_thickness == 0` рисуется только заливка.

```cpp
mngl::two_d::convex_polygon badge;
badge.points = {{0.35f, 0.3f}, {0.65f, 0.3f}, {0.8f, 0.55f}, {0.5f, 0.8f}, {0.2f, 0.55f}};
badge.fill_color = mngl::color{0.25f, 0.75f, 0.4f};
badge.outline_color = mngl::color{0.05f, 0.2f, 0.1f};
badge.outline_thickness = 0.008f;
auto frame = window.frame();
frame.draw(badge);
frame.display();
```

Для заливки произвольного невыпуклого контура задайте треугольники через `primitive_type::triangles` или подготовьте их в `triangle_strip`. `triangle_fan` соединяет первую вершину со всеми соседними парами и подходит для выпуклых контуров.

## Производительность

Спрайты и прямоугольные фигуры сохраняют прежний инстансный путь. Вершины новой геометрии преобразуются на CPU и складываются в общий динамический GPU-буфер. CPU-хранилище для триангуляции переиспользуется отдельно для renderer-а и каждого параллельного recording context. Данные загружаются один раз за кадр; GPU-буфер увеличивает ёмкость по мере необходимости и переиспользуется. Смежные геометрические объекты с одинаковыми цветом и режимом смешивания объединяются в один draw call. Порядок отрисовки не меняется.

Преобразование примитивов в треугольники выполняется за линейное время от числа вершин. `primitive_type::triangles` уже задаёт готовые треугольники и не требует дополнительной триангуляции. Сейчас один объект использует один цвет заливки; отдельные цвета или UV для каждой вершины не задаются.

Неверное число вершин для `lines` и `triangles`, а также некорректный выпуклый контур приводят к `std::invalid_argument` при рисовании объекта.
