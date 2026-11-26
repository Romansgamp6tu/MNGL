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

Если геометрия не меняется и рисуется много раз, создайте `prepared_vertex_array`. Он один раз проверяет и превращает исходный набор примитивов в неизменяемый список треугольников. `tint`, `blend` и `transform` подготовленного объекта можно менять; изменения исходного `vertex_array` на него не влияют.

```cpp
mngl::two_d::vertex_array source{mngl::two_d::primitive_type::triangle_fan};
source.add_vertex({0.5f, 0.5f})
    .add_vertex({0.5f, 0.2f})
    .add_vertex({0.8f, 0.5f})
    .add_vertex({0.5f, 0.8f})
    .add_vertex({0.2f, 0.5f});
mngl::two_d::prepared_vertex_array mesh{source};

auto frame = window.frame();
frame.draw(mesh);
frame.display();
```

`prepared_vertex_array` хранит список треугольников в CPU-памяти. Если одна и та же геометрия рисуется много кадров,
создайте `mngl::two_d::mesh`, чтобы держать вершины в постоянном GPU-буфере:

```cpp
mngl::two_d::prepared_vertex_array prepared{source};
mngl::gpu::command_buffer upload{device};
mngl::two_d::mesh gpu_mesh{device, upload, prepared};
upload.submit();

gpu_mesh.transform.translate({0.1f, 0.0f});
auto frame = window.frame();
frame.draw(gpu_mesh);
frame.display();
```

Командный буфер создания должен использовать то же устройство, что и mesh. Mesh владеет GPU-буфером и удерживает
устройство живым; командный буфер также удерживает mesh до submit или cancel. `tint`, `blend` и `transform` можно
менять без повторной загрузки вершин. Смежные отрисовки того же mesh с одинаковым состоянием рендера объединяются
через instancing.

Топология и число вершин mesh после создания фиксированы. Вершины разделены на блоки до 64 треугольников;
`block_count()`, `block_first_vertex(index)` и `block_vertex_count(index)` сообщают их диапазоны. Заменяйте только
изменённые блоки через `update_block()` или передавайте несколько изменений в `update_blocks()`:

```cpp
const std::array updates{
    mngl::two_d::mesh::block_update{0, first_changed_block},
    mngl::two_d::mesh::block_update{3, fourth_changed_block},
};
mngl::gpu::command_buffer update{device};
gpu_mesh.update_blocks(update, updates);
update.submit();
```

Для каждого обновления передавайте весь существующий блок с тем же числом вершин. Один вызов `update_blocks()`
пакует выбранные блоки вместе и записывает один copy pass. Чтобы изменить топологию или число вершин, создайте новый
mesh.

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

`mesh::update_blocks()` сохраняет неизменённые блоки и данные ранее записанных
команд через полную GPU-копию в запасной буфер перед обновлением. Передача с CPU
по-прежнему включает только заменённые вершины; GPU-копию и запасную память
следует учитывать при профилировании часто обновляемых крупных мешей.

Спрайты и прямоугольные фигуры сохраняют прежний инстансный путь. `vertex_array` предназначен для динамической геометрии: её вершины попадают в общий staging/GPU-буфер при каждом кадре. Если вершины не меняются, создайте `two_d::mesh` один раз; он держит постоянный GPU vertex buffer и каждый кадр передаёт только данные draw instance. Для примера:

```cpp
mngl::two_d::vertex_array geometry{mngl::two_d::primitive_type::triangles};
geometry.vertices = {
    {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.0f, 0.5f},
};
mngl::gpu::command_buffer upload(device);
mngl::two_d::mesh static_mesh(device, upload, geometry);
upload.submit();

// В цикле кадров:
auto frame = window.frame();
frame.draw(static_mesh);
frame.display();
```

Для изменяемого меша `update_blocks()` загружает только заменённые блоки. Transform передаётся отдельно в instance-данных и применяется вершинным шейдером, поэтому CPU не преобразует каждую вершину при каждом draw. Для отсечения CPU преобразует углы AABB каждого блока, а отдельные треугольники проверяет только у частично видимых блоков. Смежные геометрические объекты с одинаковыми transform, цветом и состоянием рендера по-прежнему объединяются; разные transform требуют отдельных draw.

Triangle-list передаётся в batcher без промежуточной копии в tessellation scratch. `prepared_vertex_array` хранит триангуляцию в CPU-памяти, а `two_d::mesh` держит вершины в постоянном GPU-буфере и передаёт каждый кадр только transform и tint. Обновление выбранных mesh-блоков загружает только их вершины. Для динамической геометрии CPU scratch переиспользуется отдельно для renderer-а и каждого параллельного recording context, а GPU-буфер растёт по мере необходимости. Порядок отрисовки не меняется.

2D benchmark по умолчанию строит геометрию в resident GPU-буфере. Передайте `--geometry-storage dynamic`, чтобы отдельно измерить путь `vertex_array`, который каждый кадр загружает вершины.

Преобразование примитивов в треугольники выполняется за линейное время от числа вершин. Проверка выпуклого контура также линейная и не вычисляет углы через `atan2`; при нулевой толщине обводка не строится. `primitive_type::triangles` уже задаёт готовые треугольники и не требует дополнительной триангуляции. Сейчас один объект использует один цвет заливки; отдельные цвета или UV для каждой вершины не задаются.

Неверное число вершин для `lines` и `triangles`, а также некорректный выпуклый контур приводят к `std::invalid_argument` при рисовании объекта.

