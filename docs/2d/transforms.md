# Трансформации 2D

Трансформация меняет положение, поворот и масштаб геометрии drawable перед применением `view` кадра. Поэтому сначала задайте камеру кадра, затем размещайте объекты в координатах этого мира. `mngl::two_d::transform` хранит affine-матрицу `glm::mat4` в плоскости XY; по умолчанию это единичная матрица, которая ничего не меняет.

У спрайта, фигур `circle`, `ellipse`, `rounded_rectangle`, заливки `rectangle_shape`, `vertex_array`, `prepared_vertex_array`, `mesh` и `convex_polygon` есть поле `transform`. Отдельные свойства фигуры (например, `circle.center`) остаются в её локальных координатах:

```cpp
mngl::two_d::circle marker{{0.5f, 0.5f}, 0.08f};
marker.transform.translate({0.15f, 0.0f})
    .rotate(0.2f, marker.center)
    .scale({1.2f, 0.8f}, marker.center);

auto frame = window.frame();
frame.draw(marker);
frame.display();
```

## Поворот вокруг центра спрайта

`sprite::rotation()` и `sprite::transform` отвечают за разные вещи. `rotation()` задаёт локальный поворот спрайта вокруг `pivot`, а `transform` затем применяется к получившейся геометрии. Удобные методы `position()`, `size()` и `rotation()` читают или меняют поля `destination`, `pivot` и `rotation_radians`:

```cpp
auto& textures = window.textures();
const auto icon_image = textures.add(mngl::texture("assets/icon.png"));
mngl::sprite icon(textures, icon_image, {0.0f, 0.0f, 1.0f, 1.0f});
icon.pivot = {0.5f, 0.5f};
icon.rotation(0.3f); // Радианы вокруг центра исходного квадрата.
icon.transform.translate({0.3f, 0.2f}).scale(0.75f);

auto frame = window.frame();
frame.draw(icon);
frame.display();
```

Для спрайта из texture atlas передайте `texture_region` вместо `image`; destination и преобразования работают так же:

```cpp
mngl::sprite icon(atlas_region, {0.0f, 0.0f, 1.0f, 1.0f});
icon.pivot = {0.5f, 0.5f};
icon.rotation(angle_radians);
icon.transform.translate(player_position);
```

В последнем примере спрайт вращается вокруг локального центра, а матрица `transform` затем размещает его в мире. Вызовы `translate()`, `rotate()` и `scale()` возвращают ссылку на ту же матрицу, поэтому их можно объединять цепочкой.

## Порядок преобразований

Каждый вызов домножает матрицу справа: `M = M * operation`. Для точки `p` рендерер применяет итоговую матрицу `M * p`, поэтому операция, добавленная позже, действует на локальную точку раньше уже накопленных операций. Порядок важен: вращение вокруг начала координат, а затем перенос обычно даёт другой результат, чем перенос с последующим вращением.

`rotate(radians, origin)` и `scale(factors, origin)` позволяют выбрать центр в локальных координатах объекта. `origin` по умолчанию `{0, 0}`; угол измеряется в радианах. Например, чтобы разместить объект в `position` и вращать вокруг локального центра `pivot`:

```cpp
mngl::two_d::transform pose;
pose.translate(position).rotate(angle_radians, pivot);
object.transform = pose;
```

Если объект уже имеет накопленную матрицу, `reset()` возвращает её к единичной. Метод `matrix()` возвращает `const glm::mat4&`, а `matrix(value)` заменяет матрицу. Допустимы только конечные affine-матрицы в плоскости XY; нечисловые значения и перспективные/трёхмерные матрицы приводят к `std::invalid_argument`.

```cpp
object.transform.reset();
object.transform.scale({1.5f, 1.0f}, pivot);
```

## Камера, локальные координаты и попадания

`transform` задаётся в координатах 2D-сцены. `view` затем отображает получившуюся точку в цель кадра. При изменении размера окна не нужно пересчитывать локальную трансформацию каждого объекта; обновите `view`, если хотите изменить видимую область мира. См. [координаты и view](coordinates-and-view.md).

Та же матрица учитывается при [hit testing](hit-testing.md). `local_coords` возвращаются в координатах исходной фигуры, до применения матрицы и поворота спрайта — это удобно, когда ввод должен работать в локальной системе объекта. При вырожденной матрице (например, нулевом масштабе по одной оси) drawable можно нарисовать, но hit testing не сможет однозначно преобразовать точку назад и не вернёт попадание.

## Стоимость

Матрица передаётся как данные объекта и применяется GPU; MNGL не переписывает все исходные вершины при каждом draw. Для `mesh` и других геометрических объектов отсечение всё равно проверяет преобразованные границы, а разные трансформации могут разделять геометрические batches. Если несколько объектов имеют одинаковый материал и render state, одинаковая матрица упрощает их объединение. Подробности см. в [произвольной геометрии](geometry.md) и [профилировании](benchmarking.md).
