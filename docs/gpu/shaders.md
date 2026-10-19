# Шейдеры

## Исходный код и байткод

`mngl::gpu::shader_loader` создаёт CPU-side assets из HLSL или GLSL:

- HLSL компилируется через `shadercross`.
- GLSL сначала компилируется в SPIR-V через `glslangValidator`; для других выходных форматов используется `shadercross`.
- `load_binary()` и `load_compute_binary()` загружают уже скомпилированный графический или compute-байткод.
- `compile_vertex()` и `compile_fragment()` задают стадию без ручной настройки `shader_compile_options`.
- `compile_compute()` использует тот же набор compile options и возвращает `compute_shader_asset`; его готовый pipeline записывается через `compute_pass`.

Обе внешние программы должны быть доступны через `PATH` или заданы в `shader_compile_options`. Целевой `shader_format` по умолчанию выбирается библиотекой для текущей платформы. Если загрузка байткода запрашивает отражение именованных uniform-полей, нужно также передать соответствующий HLSL-файл: layout восстанавливается из исходника.

После загрузки asset передаётся в `mngl::gpu::graphics_shader`, который создаёт GPU-объект на конкретном устройстве. GPU поддержка выбранного формата должна быть доступна на этом устройстве.

## Шейдер на спрайте

Для обычного 2D-кадра не нужно вручную создавать `graphics_shader`, pipeline и material instance. Назначьте fragment asset спрайту:

```cpp
auto frag = mngl::gpu::shader_loader::compile_fragment("waves.hlsl");

mngl::sprite fullscreen(win.size(), frag);

while (mngl::Input::continue_work())
{
    win.clear();
    win.draw(fullscreen);
    win.display();
}
```

Конструктор размера использует aspect ratio окна и создаёт quad по всей стандартной видимой области. Фрагментный шейдер должен быть совместим с входами 2D-batcher (`SV_Position`, `TEXCOORD0..3`); вершинный шейдер quad-геометрии MNGL подключает сам. Для спрайта с исходной текстурой назначьте asset через `image_sprite.shader(frag)`: исходная текстура автоматически назначается первому незаданному sampled-texture ресурсу; явно назначенный ресурс, например `frag["image"] = image`, имеет приоритет. Остальные ресурсы задаются в asset обычным способом.

Этот короткий путь сам создаёт и кеширует GPU material, а bindings формирует из текущего asset при первом использовании каждой пары «шейдер + исходная текстура» в кадре. Изменяйте uniforms и ресурсы до `clear()`/первого `draw()` кадра. Asset, переданный в конструктор спрайта, не копируется: он должен быть жив, когда спрайт рисуется. Для более сложных случаев по-прежнему доступны явные `renderer::create_material()` и `create_material_instance()`.

## Uniform-поля и ресурсы

`compile_fragment()` автоматически включает отражение именованных uniform-полей и ресурсов для HLSL. Для остальных случаев, например GLSL или ручного управления layout, используйте `compile()` и задайте `shader_compile_options::reflect_uniforms` явно. Поля отражённого uniform-блока можно задавать по имени или по нулевому индексу в порядке объявления:

```cpp
auto fragment = mngl::gpu::shader_loader::compile_fragment("shaders/fill.hlsl");
fragment["tint"] = glm::vec4{ 1.0f, 0.2f, 0.4f, 1.0f };
```

Поддерживаются scalar-значения, GLM-векторы и матрицы, `std::array` и ranges для массивов. Тип и количество компонентов должны соответствовать объявлению шейдера. Отражение учитывает layout MNGL для HLSL; при ручной сборке asset задайте `shader_resource_layout` самостоятельно.

Индексный setter обращается к полю uniform-блока, а не к GPU binding slot. Имена полей должны совпадать с отражёнными именами.

Имена sampled textures и storage resources тоже назначаются через `asset["name"]`. Текстуру можно присвоить напрямую (тогда используется default sampler), либо задать пару через `texture_binding`; storage buffer назначается из `mngl::gpu::buffer`.

Назначенные GPU ресурсы не копируются и не принадлежат asset или материалу. Текстуры, samplers и buffers должны оставаться живыми, пока использующий их GPU material instance может быть отправлен.

Для compute asset действует тот же интерфейс присваивания uniform-полей и ресурсов. HLSL compute layout и dispatch описаны в разделе [Compute shaders](compute.md); GLSL и бинарные shader layouts можно задать явно через `shader_compile_options`.
