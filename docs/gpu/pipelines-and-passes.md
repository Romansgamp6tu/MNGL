# Vertex layout, pipeline и render pass

Render pass говорит, **куда** рисовать. Pipeline задаёт, **как** обрабатывать вершины и фрагменты. Bindings выбирают, **какие данные** читать. Draw-команда задаёт количество элементов. Эти части нужно согласовать: одна корректная структура вершины без подходящего layout ещё не описывает вход шейдера.

## Структура вершины и layout

Начнём с позиции и цвета:

```cpp
#include <MNGL/vertex_layout.hpp>
#include <cstddef>

struct vertex
{
	float x, y;
	float r, g, b, a;
};

mngl::gpu::vertex_layout layout;
layout.add_buffer<vertex>(0)
    .add_attribute(0, 0, mngl::gpu::vertex_elemetn_fmt::float2,
                   offsetof(vertex, x))
    .add_attribute(1, 0, mngl::gpu::vertex_elemetn_fmt::float4,
                   offsetof(vertex, r));
```

`add_buffer<vertex>(0)` выводит stride из `sizeof(vertex)`; slot 0 выбирает входной vertex buffer. Первый `add_attribute` связывает shader location 0 с двумя float позиции. Второй берёт четыре float цвета по смещению `offsetof(vertex, r)`.

Location и buffer slot — разные числа. Несколько attributes могут читать один буфер, как здесь. Для instancing можно добавить другой buffer с `vertex_input_rate::instance` и отдельными attributes.

Название `vertex_elemetn_fmt` содержит опечатку в текущем публичном API; в компилируемом коде используйте именно это написание. Сама структура должна быть trivially copyable; `offsetof` удобнее ручного подсчёта байтов и учитывает фактическое расположение полей.

## Совместимые shaders

Следующие два файла подходят к описанному layout. Vertex inputs используют `TEXCOORD0` и `TEXCOORD1`; vertex output цвета передаётся fragment stage как `TEXCOORD0`.

`shaders/triangle.vert.hlsl`:

```hlsl
struct VertexInput
{
    float2 position : TEXCOORD0;
    float4 color : TEXCOORD1;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : TEXCOORD0;
};

VertexOutput main(VertexInput input)
{
    VertexOutput result;
    result.position = float4(input.position, 0.0, 1.0);
    result.color = input.color;
    return result;
}
```

`shaders/triangle.frag.hlsl`:

```hlsl
float4 main(float4 position : SV_Position, float4 color : TEXCOORD0) : SV_Target0
{
    return color;
}
```

Этот vertex shader принимает clip-space координаты; позиции в районе `[-1, 1]` попадают в видимую область. Он не использует `mngl::view` и сценовые координаты высокоуровневого 2D renderer-а. Для камеры нужно добавить собственную матрицу в shader.

Загрузка и создание GPU shaders выполняются один раз, до кадрового цикла:

```cpp
#include <MNGL/graphics_shader.hpp>
#include <MNGL/shader_loader.hpp>

auto vertex_asset =
    mngl::gpu::shader_loader::compile_vertex("shaders/triangle.vert.hlsl");
auto fragment_asset =
    mngl::gpu::shader_loader::compile_fragment("shaders/triangle.frag.hlsl");
mngl::gpu::graphics_shader vertex_shader(device, vertex_asset);
mngl::gpu::graphics_shader fragment_shader(device, fragment_asset);
```

Здесь `device` уже создан. HLSL-файлы должны существовать относительно рабочего каталога процесса, а [shader tools](shaders.md) — быть доступны загрузчику. Asset содержит CPU-байткод; `graphics_shader` создаёт соответствующий объект на выбранном GPU device.

## От вершин к render pass

Ниже функция рендерит треугольник в существующую GPU-текстуру. Передайте shaders из предыдущего примера и target с `color_target` usage того же устройства. Структура `vertex` и layout соответствуют двум файлам выше.

```cpp
#include <MNGL/gpu_buffer.hpp>
#include <MNGL/gpu_texture.hpp>
#include <MNGL/graphics_pipeline.hpp>
#include <MNGL/render_pass.hpp>
#include <MNGL/vertex_layout.hpp>

#include <array>
#include <cstddef>

void render_triangle(const mngl::gpu::device& device,
                     mngl::gpu::texture& target,
                     const mngl::gpu::graphics_shader& vertex_shader,
                     const mngl::gpu::graphics_shader& fragment_shader)
{
	struct vertex
	{
		float x, y;
		float r, g, b, a;
	};
	const std::array<vertex, 3> triangle{{
	    {-0.7f, -0.6f, 1.0f, 0.0f, 0.0f, 1.0f},
	    { 0.7f, -0.6f, 0.0f, 1.0f, 0.0f, 1.0f},
	    { 0.0f,  0.7f, 0.0f, 0.0f, 1.0f, 1.0f},
	}};

	mngl::gpu::vertex_layout layout;
	layout.add_buffer<vertex>(0)
	    .add_attribute(0, 0, mngl::gpu::vertex_elemetn_fmt::float2,
	                   offsetof(vertex, x))
	    .add_attribute(1, 0, mngl::gpu::vertex_elemetn_fmt::float4,
	                   offsetof(vertex, r));

	auto pipeline = mngl::gpu::graphics_pipeline_builder{}
	    .set_vertex_shader(vertex_shader)
	    .set_fragment_shader(fragment_shader)
	    .set_vertex_layout(layout)
	    .set_primitive_type(mngl::gpu::primitive_type::triangle_list)
	    .set_cull_mode(mngl::gpu::cull_mode::none)
	    .add_color_target(target.format())
	    .build(device);

	mngl::gpu::command_buffer commands(device);
	mngl::gpu::vertex_buffer<vertex> vertices(device, commands, triangle);
	{
		mngl::gpu::render_pass pass(
		    commands, target, mngl::color{0.05f, 0.05f, 0.08f, 1.0f});
		pass.bind_pipeline(pipeline);
		pass.bind_vertex_buffer(0, vertices);
		pass.draw_primitives(vertices.size());
	}
	commands.submit();
}
```

Сначала конструктор `vertex_buffer` записывает copy pass. После его завершения открывается render pass. При выходе из внутреннего блока проход закрывается; только затем вызывается `submit()`. Локальные GPU-объекты остаются живыми до этой отправки.

В реальном приложении pipeline и статичные vertices обычно хранятся между кадрами. В функции они собраны вместе, чтобы показать весь контракт и сделать пример независимым. Если `target` — `image.raw_gpu_texture()` от `mngl::texture`, результат затем можно вывести обычным `mngl::sprite{image}`.

## Формат цели и blending

Pipeline создаётся для формата color attachment. Если offscreen-текстура имеет `r8g8b8a8_unorm`, а swapchain — другой формат, один pipeline может не подходить к обоим. Сверяйте `target.format()` и создавайте подходящий вариант.

`add_color_target()` в примере не включает alpha blending. Для обычного прозрачного цвета используйте `add_alpha_blend_target(target.format())`. Смешивание зависит также от порядка draw: задние прозрачные объекты обычно рисуются раньше передних.

## Load/store: сохранить предыдущий рисунок

Простой render pass по умолчанию очищает цвет и сохраняет результат. Для второго прохода поверх уже нарисованного изображения задайте `loader::load`:

```cpp
mngl::gpu::render_pass overlay(
    commands, target, mngl::color{0.0f, 0.0f, 0.0f, 0.0f},
    mngl::gpu::render_pass::loader::load,
    mngl::gpu::render_pass::storer::store);
// Здесь bind_pipeline(), bindings и draw-команды второго прохода.
overlay.end_pass();
```

`commands` и `target` здесь созданы заранее; первый проход уже завершён. При `load` цвет очистки не используется. `dont_care` позволяет backend-у отбросить содержимое: этот режим подходит только тогда, когда последующие команды не читают прежние значения.

Универсальный конструктор принимает до четырёх color attachments и необязательный depth/stencil attachment. Их форматы и количество должны совпадать с pipeline. Depth-target требует соответствующего depth-формата и usage `stencil_target`; builder pipeline настраивается через `set_depth_stencil_format()` и `enable_depth_test()`.

## Uniforms и индексированная геометрия

`push_vertex_uniform(slot, data)` и `push_fragment_uniform(slot, data)` передают trivially copyable данные в command buffer активного прохода. Slot должен совпадать с shader binding, а размер, типы и padding — с shader layout. Для сложных HLSL uniform-блоков полезно [именованное отражение](shaders.md#uniform-поля-и-ресурсы); ручная C++-структура требует проверки offsets, особенно для `vec3`, `vec4` и матриц.

После `end_pass()`, перемещения или для пустого `render_pass` draw-, bind- и uniform-вызовы сообщают `std::logic_error`, не передавая недействительный SDL handle. Поэтому эти операции не объявлены `noexcept`.

Для indexed draw сначала свяжите vertex buffer, затем index buffer и вызовите `draw_indexed_primitives()`. Размер индекса задаётся в binding; 32-битному массиву соответствует `SDL_GPU_INDEXELEMENTSIZE_32BIT`. Подготовка индексов показана в [ресурсах](resources.md#обычный-buffer-и-usage).

## Если вместо треугольника ничего нет

Проверьте, что проход завершён до `submit()`, target имеет нужный usage, pipeline построен для его формата, attributes используют правильные location/offset/stride, а draw получает число вершин. После этого проверьте clip-space позиции, culling и shader inputs. Ошибка pipeline включает shader names, layouts и параметры targets — сохраняйте полный диагностический текст.

