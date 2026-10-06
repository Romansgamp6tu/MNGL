# Трансформации 2D

`mngl::two_d::transform` хранит affine-матрицу `glm::mat4` в плоскости XY. Матрица по умолчанию единичная, поэтому не меняет существующее положение объекта. Она применяется к геометрии объекта перед проекцией текущего `view`.

У спрайта и фигур `circle`, `ellipse` и `rounded_rectangle` поле `transform` имеет один тип — `mngl::two_d::transform`:

```cpp
mngl::two_d::circle marker{ { 0.5f, 0.5f }, 0.08f };
marker.transform
    .translate({ 0.15f, 0.0f })
    .rotate(0.2f, marker.center)
    .scale({ 1.2f, 0.8f });

window.clear();
window.draw(marker);
window.display();
```

Геометрия спрайта задаётся отдельно через `destination`, `pivot` и `rotation_radians`; удобные методы `position()`, `size()` и `rotation()` читают и меняют соответствующие значения. `rotation()` задаёт локальный угол вокруг `pivot`, а матрица `transform` применяется поверх этой геометрии:

```cpp
mngl::sprite icon(image, { 0.2f, 0.2f, 0.3f, 0.3f });
icon.position({ 0.3f, 0.2f }).size({ 0.3f, 0.3f }).rotation(0.15f);
icon.transform.translate({ 0.1f, 0.0f }).scale(1.1f);
window.draw(icon);
```

Для текстуры из атласа используйте такой же `destination`:

```cpp
mngl::sprite icon(atlas_region, { 0.2f, 0.2f, 0.3f, 0.3f });
```

Для немедленных draw-вызовов передайте трансформацию последним аргументом: `frame.draw(bounds, tint, transform)` или `frame.draw(texture, bounds, tint, transform)`. То же доступно у `render_window` и render-target `texture`.

`translate`, `rotate` и `scale` домножают матрицу справа; координаты задаются в единицах текущего 2D мира, угол — в радианах. `rotate` и `scale` принимают необязательный центр преобразования. Матрицу GLM можно прочитать через `matrix()` или установить через `matrix(value)`; разрешены только конечные affine-матрицы в плоскости XY.

Matrix-трансформация учитывается и при hit-testing: `local_coords` возвращаются в единицах исходной геометрии, до применения матрицы. Объект с вырожденной матрицей рисуется как обычно, но не может быть hit-test целью.
