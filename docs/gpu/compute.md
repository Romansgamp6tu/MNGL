# Вычислительные шейдеры (compute shaders)

Compute shader запускается для набора потоков и записывает данные в GPU-ресурс. В этом примере каждый поток заполняет один пиксель; результат затем отображается обычным спрайтом. Начните с [устройства и команд](device-and-commands.md) и [shader tools](shaders.md), если это первый GPU-пример.

Загрузка compute-шейдеров использует те же `shader_compile_options`, выбор языка, компиляторы, форматы байткода и интерфейс именованных asset-полей, что и графические шейдеры. `shader_loader::compile_compute()` возвращает `compute_shader_asset`; `compute_pipeline_builder` создаёт из него GPU pipeline, а `compute_pass` записывает dispatch-команды в `command_buffer`.

<a id="hlsl-texture-writing"></a>

## HLSL: запись в текстуру

По соглашению MNGL HLSL reflection использует `b0, space2` для compute uniform-блока. Read-only textures, buffers и samplers находятся в `space0`, writable `RWTexture2D` и `RWStructuredBuffer` — в `space1`. Это соглашение отличается от fragment shader: его uniform-блок находится в `b0, space3`, а ресурсы — в `space2`.

```hlsl
RWTexture2D<float4> output : register(u0, space1);

cbuffer Params : register(b0, space2)
{
    uint2 size;
    float4 color;
};

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x >= size.x || thread_id.y >= size.y)
    {
        return;
    }

    output[thread_id.xy] = color;
}
```

Загрузчик автоматически определяет literal-размер `[numthreads]` и отражает поддерживаемые HLSL-ресурсы. В примере `window` — уже созданный `mngl::render_window`; uniforms и ресурсы задаются так же, как для графического asset:

Ресурсы `t` используют общее пространство регистров для sampled textures и read-only buffers; `u` — для writable textures и buffers. Номера внутри каждого набора должны быть непрерывными от нуля, а SDL slots затем нумеруются отдельно для каждого вида ресурса. Например, `Texture2D` в `t0` и `StructuredBuffer` в `t1` получают по slot `0`. Отражение не запускает HLSL preprocessor: исходник с `#include`, условными директивами (`#if`, `#ifdef` и т. п.) или макросами (`#define`, `#undef`) завершается ошибкой. Это также относится к автоматическому чтению `[numthreads]`. Передайте готовый HLSL без этих директив; для уже скомпилированного compute bytecode можно не передавать исходник, указать `compute_threadgroup_size`, задать counts вручную и оставить `reflect_uniforms = false`.

```cpp
constexpr uint32_t width = 512;
constexpr uint32_t height = 512;
auto& device = mngl::gpu::device::global();

mngl::texture output{device, width, height, mngl::gpu::texture_format::r8g8b8a8_unorm,
                     mngl::texture::usage::sampler | mngl::texture::usage::compute_write};

auto shader = mngl::gpu::shader_loader::compile_compute("shaders/fill.hlsl");
shader["output"] = output;
shader["size"] = glm::uvec2{width, height};
shader["color"] = glm::vec4{0.1f, 0.4f, 0.9f, 1.0f};

auto pipeline = mngl::gpu::compute_pipeline_builder{}.set_compute_shader(shader).build(device);

{
	mngl::gpu::command_buffer commands(device);
	mngl::gpu::compute_pass pass(commands, pipeline, shader);
	pass.dispatch_for({width, height, 1});
	pass.end_pass();
	commands.submit();
}

auto output_sprite = mngl::sprite::borrowed(output);
window.clear();
window.draw(output_sprite);
window.display();
```

Конструктор pass, принимающий asset, связывает отражённые ресурсы и перед dispatch отправляет uniform-блок. `dispatch_for()` принимает число потоков шейдера и округляет размеры вверх до целого количества workgroup; шейдер всё равно должен проверять границы dispatch ID. `dispatch(x, y, z)` принимает количество workgroup напрямую. Compute pass завершается при уничтожении или вызове `end_pass()`; завершите его до отправки command buffer.

Для HLSL reflection поддерживает один `cbuffer` в `b0, space2`. Read-only `Texture2D`, `StructuredBuffer` и `SamplerState` размещаются в `space0`; writable `RWTexture2D` и `RWStructuredBuffer` — в `space1`. Sampled-текстуры связываются с sampler на том же register index; слоты каждого типа должны идти подряд с нуля. Назначьте поля ресурсов до создания pass. Для sampled-текстуры без явно заданного sampler можно передать default sampler в `compute_pass`.

<a id="glsl-bytecode-bindings"></a>

## GLSL, байткод и явные bindings

`compile_compute()` также принимает GLSL при `source_language = shader_source_language::glsl`: для SPIR-V используется `glslangValidator`, для других форматов — `shadercross`. Размеры из literal-объявления `layout(local_size_x = ..., local_size_y = ..., local_size_z = ...) in;` определяются автоматически. Для макросов и других нелитеральных объявлений задайте `compute_threadgroup_size` явно. `load_compute_binary()` принимает ту же опцию; укажите размер workgroup либо matching source, из которого его можно определить. Если заданы и явный размер, и исходник с распознаваемым literal-размером, загрузчик сверяет их и отклоняет несовпадение. Если размер исходника определить нельзя, явное значение остаётся fallback.

Для GLSL и precompiled assets количество ресурсов pipeline задаётся в `shader_compile_options::resources`. Compute-специфичные поля — `read_write_storage_textures` и `read_write_storage_buffers`; также доступны `samplers`, `storage_textures`, `storage_buffers` и `uniform_buffers`. Для таких layout используйте slot-based bindings:

```cpp
std::array outputs{mngl::gpu::compute_pass::storage_texture_write_binding{output}};

mngl::gpu::command_buffer commands(device);
mngl::gpu::compute_pass pass(commands, pipeline, outputs);
pass.bind_storage_texture(0, input);
pass.push_uniform(0, parameters);
pass.dispatch(64, 64, 1);
pass.end_pass();
commands.submit();
```

Read-write ресурсы нужно передать при создании pass, поскольку SDL связывает их в `SDL_BeginGPUComputePass`; элементы явных массивов должны идти в порядке shader slots. Read-only storage-текстуры/буферы и samplers связываются после создания pass. Их usage должен включать соответственно `compute_read`, `compute_storage_read` или `sampler`; для writable-текстур нужен `compute_write` или `compute_rw`, для writable-буферов — `compute_storage_write`. Все ресурсы и pipeline должны принадлежать устройству command buffer и оставаться живыми до его отправки.

Compute-записи остаются на GPU: текстуру с usage `sampler` и compute-write можно читать обычным 2D renderer без CPU readback. Если dispatch-команды должны последовательно записывать пересекающиеся области ресурсов, используйте отдельные compute pass: SDL не гарантирует порядок таких записей внутри одного pass.

Для автоматического порядка между compute- и render-проходами и владения временными текстурами/буферами см. [граф кадра](frame-graph.md). Для обычной 2D-композиции после одного compute submit граф не обязателен.

## Полное приложение для fill.hlsl

Сохраните HLSL из первого раздела в `shaders/fill.hlsl` рядом с рабочим каталогом запуска. Следующий `main.cpp` создаёт окно, один раз компилирует shader/pipeline, затем каждый кадр обновляет цвет, запускает compute и выводит результат. Проект должен ссылаться на `MNGL::MNGL`; настройка CMake одинакова с [первым приложением](../getting-started/using-mngl.md).

```cpp
#include <MNGL/compute_pass.hpp>
#include <MNGL/compute_pipeline.hpp>
#include <MNGL/input.hpp>
#include <MNGL/render_window.hpp>
#include <MNGL/shader_loader.hpp>
#include <MNGL/texture.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::render_window window("Compute fill", {800, 600});
		const auto& device = mngl::gpu::device::global();
		constexpr std::uint32_t width = 512;
		constexpr std::uint32_t height = 512;
		using usage = mngl::texture::usage;

		mngl::texture output(
		    device, width, height, mngl::gpu::texture_format::r8g8b8a8_unorm,
		    usage::sampler | usage::compute_write);
		auto shader =
		    mngl::gpu::shader_loader::compile_compute("shaders/fill.hlsl");
		shader["output"] = output;
		shader["size"] = glm::uvec2{width, height};
		auto pipeline = mngl::gpu::compute_pipeline_builder{}
		    .set_compute_shader(shader)
		    .build(device);
		auto image = mngl::sprite::borrowed(output, {0.0f, 0.0f, 1.0f, 1.0f});

		double elapsed = 0.0;
		while (mngl::input::next_frame())
		{
			elapsed += mngl::input::delta_time();
			const float blue = 0.5f + 0.5f * static_cast<float>(std::sin(elapsed));
			shader["color"] = glm::vec4{0.1f, 0.4f, blue, 1.0f};

			mngl::gpu::command_buffer commands(device);
			{
				mngl::gpu::compute_pass pass(commands, pipeline, shader);
				pass.dispatch_for({width, height, 1});
			}
			commands.submit();

			auto frame = window.frame();
			frame.draw(image);
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

Ожидаемый результат — квадрат с плавно меняющимся синим каналом. Shader и pipeline создаются до цикла: повторная компиляция HLSL каждый кадр не нужна. Ресурс `output` живёт дольше спрайта и всех команд, которые к нему обращаются.

В конце compute scope деструктор закрывает pass, после чего `submit()` отправляет запись. Затем обычный 2D-кадр читает ту же текстуру на том же глобальном device. Копирование на CPU между compute и draw не выполняется.

## Число потоков и число групп — разные величины

Для `[numthreads(8, 8, 1)]` вызов `dispatch_for({512, 512, 1})` даёт 64 × 64 × 1 группы. Если размер изображения 513 × 513, нужно уже 65 × 65 групп. Лишние потоки выйдут за границы изображения, поэтому проверка `thread_id.x >= size.x` и `thread_id.y >= size.y` в HLSL обязательна.

`dispatch(64, 64, 1)` указывает непосредственно группы. Не передавайте туда размеры изображения, если не собираетесь запустить столько групп. Нулевые размеры dispatch недопустимы. `dispatch_indirect()` читает аргументы из буфера с usage `indirect`; offset задаётся в байтах и должен быть выровнен на `uint32_t`.

## Checklist bindings перед dispatch

| Shader declaration | Usage/действие в C++ |
|---|---|
| `RWTexture2D` | `compute_write` или `compute_rw`; binding задаётся при создании pass |
| Read-only storage texture | `compute_read`; `bind_storage_texture()` |
| `RWStructuredBuffer` | `compute_storage_write`; binding при создании pass |
| Read-only storage buffer | `compute_storage_read`; `bind_storage_buffer()` |
| Sampled texture | `sampler` и sampler того же device |
| Uniform block | Именованные поля asset или `push_uniform()` с совместимым layout |

`compute_pass` проверяет, что все требуемые pipeline-ресурсы связаны до dispatch. Ошибка отсутствующего binding означает, что нужно сравнить layout shader-а с назначенными полями и slots, а не добавлять ещё один `submit()`.

## Изменение размера и цепочка вычислений

При изменении размера output создайте новую текстуру, обновите `shader["output"]`, uniform `size` и источник спрайта до нового dispatch. Одного изменения uniform недостаточно: оно не меняет выделенный размер GPU-текстуры.

Для последовательных вычислений «проход A записывает, проход B читает результат» завершайте A перед созданием B. Если несколько dispatch записывают пересекающиеся области, не рассчитывайте на порядок записи внутри одного compute pass. Для более сложных зависимостей и временных ресурсов используйте [граф кадра](frame-graph.md).

Ошибка загрузчика обычно связана с путём к HLSL, отсутствием compiler tools или неподдерживаемым output format. Ошибка создания pass/dispatch чаще указывает на usage, device, layout или незаданный ресурс. Сохраняйте полный текст исключения: compute pipeline сообщает threadgroup size и число ресурсов.
