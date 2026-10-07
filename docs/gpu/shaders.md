# Шейдеры

## Исходный код и байткод

`mngl::gpu::shader_loader` создаёт CPU-side `graphics_shader_asset` из HLSL или GLSL:

- HLSL компилируется через `shadercross`.
- GLSL сначала компилируется в SPIR-V через `glslangValidator`; для других выходных форматов используется `shadercross`.
- `load_binary()` загружает уже скомпилированный байткод.

Обе внешние программы должны быть доступны через `PATH` или заданы в `shader_compile_options`. Целевой `shader_format` по умолчанию выбирается библиотекой для текущей платформы. Если загрузка байткода запрашивает отражение именованных uniform-полей, нужно также передать соответствующий HLSL-файл: layout восстанавливается из исходника.

После загрузки asset передаётся в `mngl::gpu::graphics_shader`, который создаёт GPU-объект на конкретном устройстве. GPU поддержка выбранного формата должна быть доступна на этом устройстве.

## Uniform-поля и ресурсы

Когда отражение включено через `shader_compile_options::reflect_uniforms`, поля uniform-блока можно задавать по имени или по нулевому индексу в порядке объявления:

```cpp
options.reflect_uniforms = true;
auto fragment = mngl::gpu::shader_loader::compile("shaders/fill.hlsl", options);
fragment["tint"] = glm::vec4{ 1.0f, 0.2f, 0.4f, 1.0f };
```

Поддерживаются scalar-значения, GLM-векторы и матрицы, `std::array` и ranges для массивов. Тип и количество компонентов должны соответствовать объявлению шейдера. Отражение учитывает layout MNGL для HLSL; при ручной сборке asset задайте `shader_resource_layout` самостоятельно.

Индексный setter обращается к полю uniform-блока, а не к GPU binding slot. Имена полей должны совпадать с отражёнными именами.

Имена sampled textures и storage resources тоже назначаются через `asset["name"]`. Текстуру можно присвоить напрямую (тогда используется default sampler), либо задать пару через `texture_binding`; storage buffer назначается из `mngl::gpu::buffer`.

Назначенные GPU ресурсы не копируются и не принадлежат asset или материалу. Текстуры, samplers и buffers должны оставаться живыми, пока использующий их GPU material instance может быть отправлен.
