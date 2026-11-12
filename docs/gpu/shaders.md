# Шейдеры

## Исходный код и байткод

`mngl::gpu::shader_loader` создаёт CPU-side assets из HLSL или GLSL:

- На Windows и Linux HLSL компилируется через `shadercross` и DXC. На macOS
  MNGL компилирует HLSL в SPIR-V через `glslangValidator`, затем преобразует
  SPIR-V в MSL через `shadercross`/SPIRV-Cross.
- GLSL сначала компилируется в SPIR-V через `glslangValidator`; для других выходных форматов используется `shadercross`.
- `load_binary()` и `load_compute_binary()` загружают уже скомпилированный графический или compute-байткод.
- `compile_vertex()` и `compile_fragment()` задают стадию без ручной настройки `shader_compile_options`.
- `compile_compute()` использует тот же набор compile options и возвращает `compute_shader_asset`; его готовый pipeline записывается через `compute_pass`.

`shadercross` и `glslangValidator` должны быть доступны через `PATH` или заданы в
`shader_compile_options`. На Windows/Linux для HLSL достаточно shadercross с
DXC; на macOS нужны оба инструмента. Целевой `shader_format` по умолчанию
выбирается библиотекой для текущей платформы. Если загрузка байткода запрашивает
отражение именованных uniform-полей, нужно также передать соответствующий
HLSL-файл: layout восстанавливается из исходника.

После загрузки asset передаётся в `mngl::gpu::graphics_shader`, который создаёт GPU-объект на конкретном устройстве. GPU поддержка выбранного формата должна быть доступна на этом устройстве.

## Шейдер на спрайте

Для обычного 2D-кадра не нужно вручную создавать `graphics_shader`, pipeline и material instance. Назначьте fragment asset спрайту:

```cpp
auto frag = mngl::gpu::shader_loader::compile_fragment("waves.hlsl");

mngl::sprite fullscreen(win.size(), frag);

while (mngl::input::next_frame())
{
	win.clear();
	win.draw(fullscreen);
	win.display();
}
```

Конструктор размера использует aspect ratio окна и создаёт quad по всей стандартной видимой области. Фрагментный шейдер должен быть совместим с входами 2D-batcher (`SV_Position`, `TEXCOORD0..3`); вершинный шейдер quad-геометрии MNGL подключает сам. Для спрайта с исходной текстурой назначьте asset через `image_sprite.shader(frag)`: исходная текстура автоматически назначается первому незаданному sampled-texture ресурсу; явно назначенный ресурс, например `frag["image"] = image`, имеет приоритет. Остальные ресурсы задаются в asset обычным способом.

Этот короткий путь сам создаёт и кеширует GPU material, а bindings формирует из текущего asset при первом использовании каждой пары «шейдер + исходная текстура» в кадре. Изменяйте uniforms и ресурсы до `clear()`/первого `draw()` кадра. Asset, переданный в конструктор спрайта, не копируется: он должен быть жив, когда спрайт рисуется. Для более сложных случаев по-прежнему доступны явные `renderer::create_material()` и `create_material_instance()`.

## Первый fragment shader целиком

Начните со сплошного цвета: так для первого shader-спрайта не нужны PNG и texture bindings. Сохраните файл как `shaders/tint.hlsl`:

```hlsl
cbuffer TintParams : register(b0, space3)
{
    float4 tint;
};

float4 main(float4 position : SV_Position) : SV_Target0
{
    return tint;
}
```

Создайте asset до кадра, задайте именованное поле и передайте asset спрайту:

```cpp
#include <MNGL/input.hpp>
#include <MNGL/render_window.hpp>
#include <MNGL/shader_loader.hpp>

#include <exception>
#include <iostream>

int main()
{
    try
    {
        mngl::render_window window("Tint shader", {800, 600});
        auto tint = mngl::gpu::shader_loader::compile_fragment("shaders/tint.hlsl");
        tint["tint"] = glm::vec4{0.15f, 0.7f, 0.95f, 1.0f};
        mngl::sprite background{window.size(), tint};

        while (mngl::input::next_frame())
        {
            auto frame = window.frame(mngl::color{0.05f, 0.06f, 0.1f, 1.0f});
            frame.draw(background);
            frame.display();
        }
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

Запускайте приложение из каталога, где существует `shaders/tint.hlsl`, и убедитесь, что runtime-компилятор доступен в `PATH` или задан в `shader_compile_options`. Спрайт хранит невладеющую ссылку на `tint`; поэтому asset объявлен раньше спрайта и живёт весь цикл. Такой приём полезен для полноэкранного градиента, виньетки или цветовой коррекции; для картинки возьмите пример sampled texture из раздела ниже.

<a id="uniform-fields-and-resources"></a>

## Uniform-поля и ресурсы

`compile_fragment()` автоматически включает отражение именованных uniform-полей и ресурсов для HLSL. Отражатель принимает один `cbuffer` в `register(b0, space3)` для графического fragment shader и понимает scalar-, vector- и matrix-поля, а также фиксированные массивы таких значений. Для compute shader соглашение другое: uniform-block находится в `register(b0, space2)`; см. [таблицу slots](compute.md#glsl-bytecode-bindings). Reflection для GLSL не реализован: у GLSL задайте число ресурсов в `shader_compile_options::resources` и используйте ручные bindings.

Имена uniforms доступны по строке или по нулевому индексу в порядке объявления:

```cpp
auto fragment = mngl::gpu::shader_loader::compile_fragment("shaders/fill.hlsl");
fragment["tint"] = glm::vec4{1.0f, 0.2f, 0.4f, 1.0f};
```

Поддерживаются scalar-значения, GLM-векторы и матрицы, `std::array` и ranges для массивов. Тип и количество компонентов должны соответствовать объявлению шейдера. Отражение учитывает layout MNGL для HLSL; при ручной сборке asset задайте `shader_resource_layout` самостоятельно.

Индексный setter обращается к полю uniform-блока, а не к GPU binding slot. Имена полей должны совпадать с отражёнными именами.

Имена sampled textures и storage resources тоже назначаются через `asset["name"]`. Текстуру можно присвоить напрямую (тогда используется default sampler), либо задать пару через `texture_binding`; storage buffer назначается из `mngl::gpu::buffer`.

Назначенные GPU ресурсы не копируются и не принадлежат asset или материалу. Текстуры, samplers и buffers должны оставаться живыми, пока использующий их GPU material instance может быть отправлен.

Минимальные HLSL-объявления для пары «текстура + sampler»:

```hlsl
Texture2D<float4> image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);
```

В C++ назовите ресурс точно как в HLSL и задайте sampled texture с собственным sampler:

```cpp
auto fragment = mngl::gpu::shader_loader::compile_fragment("shaders/sprite.hlsl");
fragment["tint"] = glm::vec4{1.0f, 0.9f, 0.8f, 1.0f};
fragment["image"] = mngl::gpu::texture_binding{image, linear_sampler};
```

Для отражения shader должен объявить `Texture2D` и парный `SamplerState` с одинаковым индексом register в `space2`; `image` — точное имя поля HLSL. Если sampler не задавать явно, обычный 2D renderer подставляет свой default sampler. У sprite с исходной текстурой MNGL автоматически связывает source texture с первым ещё не назначенным sampled-texture полем. Остальные resources следует назначить до начала кадра.

## Выбор компилятора и готового bytecode

`shader_compile_options` задаёт язык исходника, желаемый `shader_format`, entry point и пути к программам компиляторов. Например, когда runtime tools лежат в нестандартном каталоге:

```cpp
mngl::gpu::shader_compile_options options{};
options.shadercross_executable = "tools/shadercross";
options.glslang_validator_executable = "tools/glslangValidator";
auto fragment = mngl::gpu::shader_loader::compile_fragment("shaders/tint.hlsl", options);
```

Путь может быть абсолютным или находиться в `PATH`. На Windows для bare filename библиотека сначала ищет exe рядом с приложением, затем ищет его средствами ОС. Для HLSL на Windows/Linux используется SDL_shadercross; для HLSL на macOS нужен `glslangValidator` для SPIR-V и `shadercross` для преобразования в MSL. GLSL всегда сначала проходит через `glslangValidator`; shadercross требуется, если итоговый формат не SPIR-V. Подробные runtime-файлы перечислены в [руководстве сборки](../getting-started/building.md#shader-compiler-runtime).

Для заранее собранного bytecode укажите stage и формат явно:

```cpp
mngl::gpu::shader_compile_options options{};
options.stage = mngl::gpu::shader_stage::fragment;
options.format = mngl::gpu::shader_format::spirv;
auto fragment = mngl::gpu::shader_loader::load_binary(
    "shaders/tint.spv", "shaders/tint.hlsl", options);
```

`load_binary()` не компилирует исходник: байткод должен совпадать с текущим stage/format и поддерживаться выбранным device. Если нужны имена uniforms, передайте matching HLSL как второй аргумент и включите `options.reflect_uniforms = true`; без исходника загрузчик не может восстановить этот layout. Для compute bytecode также задайте размер threadgroup или передайте исходник, из которого он будет прочитан: подробности есть в [руководстве compute](compute.md).

Для compute asset действует тот же интерфейс присваивания uniform-полей и ресурсов. HLSL compute layout и dispatch описаны в разделе [Compute shaders](compute.md); GLSL и бинарные shader layouts можно задать явно через `shader_compile_options`.

