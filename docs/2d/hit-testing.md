# Попадания и локальные координаты мыши

Hit testing выключен по умолчанию. Включите его до начала кадра у окна и у каждой render-target текстуры, через которую нужно пройти:

```cpp
window.enable_hit_testing();
panel.enable_hit_testing();
```

Окно записывает компактные данные вызовов `draw()` только для включённых целей. Для поиска передайте позицию мыши в логических координатах окна SDL: начало координат находится в левом верхнем углу.

```cpp
if (const auto hit = window.hit_test(mngl::Input::get_mouse_position()))
{
    const auto& deepest = hit->path.back();
    const glm::vec2 local = deepest.local_coords;
}
```

`path` идёт от объекта в окне к самому глубокому объекту в отображаемой render-target текстуре. Каждый элемент содержит `draw_index` — индекс вызова рисования в соответствующей цели — и координаты `local_coords` в локальном пространстве фигуры: начало в левом нижнем углу, единицы совпадают с размером фигуры. Поворот и pivot учитываются. Если объекты перекрываются, возвращается верхний по порядку рисования; для вложенной цели путь содержит и спрайт-текстуру, и найденный внутри неё объект.

Для рекурсивного поиска включите hit testing на каждом render target в цепочке. Текстуры без сохранённого hit tree остаются обычными листами: результат остановится на спрайте, который их отображает. Используется последний успешно завершённый кадр.

Попадание в текстурированный quad проверяется по его геометрии и scissor, без чтения alpha-пикселей с GPU. Поэтому прозрачные участки текстуры и tint тоже считаются частью спрайта. Для кругов, эллипсов и скруглённых прямоугольников проверяется форма, а не только её bounding box.

## Обработка клика

Чтобы вызывать обработчик только при нажатии, назначьте его drawable через `on_click()` и явно вызовите `window.dispatch_click()` из цикла ввода:

```cpp
mngl::two_d::rounded_rectangle button{ { 0.2f, 0.2f, 0.4f, 0.2f }, 0.05f };
button.on_click([](
    const mngl::hit_test_result&,
    const mngl::hit_test_path_entry& clicked)
{
    const glm::vec2 local = clicked.local_coords;
});

bool has_completed_frame = false;
while (mngl::Input::continue_work())
{
    if (has_completed_frame &&
        mngl::Input::is_mouse_button_pressed(mngl::mouse_button::left))
    {
        (void)window.dispatch_click(mngl::Input::get_mouse_position());
    }
    window.clear();
    // Draw the next frame here.
    window.display();
    has_completed_frame = true;
}
```

`on_click()` доступен у `mngl::sprite`, `mngl::two_d::circle`, `ellipse` и `rounded_rectangle`. Обработчик вызывается синхронно при `dispatch_click()` и получает весь путь попадания и запись drawable, которому принадлежит обработчик; `clicked.local_coords` — локальные координаты этого drawable. `dispatch_click()` возвращает `true`, если обработчик был вызван.

Обрабатывается верхнее по порядку рисования попадание. Если у него нет обработчика, вызов поднимается по вложенному пути до ближайшего clickable-предка; к перекрывающимся нижним объектам он не переходит. Hit testing должен быть включён у окна и у каждого render target в цепочке. Диспетчеризация использует последний успешно завершённый кадр, поэтому её удобно выполнять после получения события ввода и до рисования следующего кадра.
