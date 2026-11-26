# Попадания и локальные координаты мыши

Hit testing выключен по умолчанию. Включите его до начала кадра у окна и у каждой render-target текстуры, через которую нужно пройти:

```cpp
window.enable_hit_testing();
panel.enable_hit_testing();
```

Окно записывает компактные данные вызовов `draw()` только для включённых целей. Для поиска передайте позицию мыши в логических координатах окна SDL: начало координат находится в левом верхнем углу.

```cpp
for (const auto& mouse : mngl::input::mice())
{
	if (const auto hit = window.hit_test(mouse.position()))
	{
		const auto& deepest = hit->path.back();
		const glm::vec2 local = deepest.local_coords;
	}
}
```

`path` идёт от объекта в окне к самому глубокому объекту в отображаемой render-target текстуре. Каждый элемент содержит `draw_index` — индекс вызова рисования в соответствующей цели — и координаты `local_coords` в локальном пространстве фигуры: начало в левом нижнем углу, единицы совпадают с размером фигуры. Поворот и pivot учитываются. Если объекты перекрываются, возвращается верхний по порядку рисования; для вложенной цели путь содержит и спрайт-текстуру, и найденный внутри неё объект.

Для текстурированного drawable запись пути также содержит непрозрачный `source_identity`, размер источника и `source_pixel_coords` в пикселях исходной текстуры с началом координат в левом нижнем углу. Координаты уже учитывают UV-область спрайта; их можно использовать, например, чтобы направить ввод в ImGui-поверхность, нарисованную как спрайт.

Для рекурсивного поиска включите hit testing на каждом render target в цепочке. Текстуры без сохранённого hit tree остаются обычными листами: результат остановится на спрайте, который их отображает. Используется последний успешно завершённый кадр.

Промежуточный offscreen-кадр через `render_window::clear(target)` не заменяет hit tree окна: он обновляется только после отображения кадра в swapchain.

Попадание в текстурированный quad проверяется по его геометрии и scissor, без чтения alpha-пикселей с GPU. Поэтому прозрачные участки текстуры и tint тоже считаются частью спрайта. Для кругов, эллипсов и скруглённых прямоугольников проверяется форма, а не только её bounding box.

## Геометрия SVG

Чтобы получать SVG-элемент под курсором, включите сохранение hit regions при загрузке документа и hit testing у окна:

```cpp
mngl::two_d::svg_load_options options;
options.hit_testable = true;
auto icon = mngl::two_d::svg_object::from_file("assets/icon.svg", options);
icon.on_click([](const mngl::hit_test_result&, const mngl::hit_test_path_entry& clicked)
              {
                  if (clicked.svg_element)
                  {
                      const auto& element = *clicked.svg_element;
                      // element.id, element.name и element.index
                  }
              });

window.enable_hit_testing();
```

CPU-объект и созданный из загруженного объекта `svg_sprite` проверяются одинаково. Для спрайта параметр `hit_testable` задаётся до GPU-загрузки исходного `svg_object` либо передаётся в `svg_sprite::from_file()`. Как и для остальных объектов, вызовите `window.hit_test()` для поиска или `window.dispatch_click()` для вызова `on_click()`.

`hit_test_path_entry::svg_element` содержит `id` SVG (пустую строку, если `id` не задан), имя тега (`path`, `rect`, `circle` и другие drawable-элементы) и нулевой `index` фигуры в порядке разбора с учётом раскрытия `<use>`. Заливка, обводка и несколько под-контуров одной фигуры возвращают один и тот же индекс. Узел `<g>` отдельно не является областью попадания: результат указывает на конкретную рисуемую фигуру внутри группы. `local_coords` в такой записи даны в координатах path этой фигуры.

Попадание проверяется по треугольникам заливки и обводки с учётом трансформаций и scissor. Поэтому отверстия, заданные правилом заливки, остаются промахами. Пиксельная прозрачность, цвет и opacity не проверяются; для кликабельной области по непрозрачности используйте отдельную геометрию. Сохранение SVG hit regions выключено по умолчанию; глобальный hit testing цели тоже должен быть включён.

Маркеры возвращают ID фигуры, на которой стоят. У `<image>` проверяется видимый прямоугольник после применения `preserveAspectRatio`. Альфа маски и пиксельные эффекты фильтра влияют на отрисовку. Для попадания используется исходная геометрия фигуры.

## Обработка клика

Чтобы вызывать обработчик только при нажатии, назначьте его drawable через `on_click()` и явно вызовите `window.dispatch_click()` из цикла ввода:

```cpp
mngl::two_d::rounded_rectangle button{{0.2f, 0.2f, 0.4f, 0.2f}, 0.05f};
button.on_click([](const mngl::hit_test_result&, const mngl::hit_test_path_entry& clicked)
                { const glm::vec2 local = clicked.local_coords; });

bool has_completed_frame = false;
while (mngl::input::next_frame())
{
	const auto mice = mngl::input::mice();
	if (has_completed_frame)
	{
		for (const auto& mouse : mice)
		{
			if (mouse.pressed(mngl::mouse_button::left))
			{
				(void)window.dispatch_click(mouse.position());
			}
		}
	}
	window.clear();
	// Draw the next frame here.
	window.display();
	has_completed_frame = true;
}
```

При нескольких мышах обработчик получает отдельный клик и позицию от каждой.

`on_click()` доступен у `mngl::sprite`, `mngl::two_d::circle`, `ellipse`, `rounded_rectangle`, `svg_object` и `svg_sprite`. Для SVG также включите `svg_load_options::hit_testable`. Обработчик вызывается синхронно при `dispatch_click()` и получает весь путь попадания и запись drawable, которому принадлежит обработчик; у SVG `clicked.svg_element` содержит найденную фигуру, а `clicked.local_coords` — её локальные координаты. `dispatch_click()` возвращает `true`, если обработчик был вызван.

Обрабатывается верхнее по порядку рисования попадание. Если у него нет обработчика, вызов поднимается по вложенному пути до ближайшего clickable-предка; к перекрывающимся нижним объектам он не переходит. Hit testing должен быть включён у окна и у каждого render target в цепочке. Диспетчеризация использует последний успешно завершённый кадр, поэтому её удобно выполнять после получения события ввода и до рисования следующего кадра.

