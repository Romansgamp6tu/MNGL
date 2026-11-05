# Вычислительные шейдеры (compute shaders)

Загрузка compute-шейдеров использует те же `shader_compile_options`, выбор языка, компиляторы, форматы байткода и интерфейс именованных asset-полей, что и графические шейдеры. `shader_loader::compile_compute()` возвращает `compute_shader_asset`; `compute_pipeline_builder` создаёт из него GPU pipeline, а `compute_pass` записывает dispatch-команды в `command_buffer`.

## HLSL: запись в текстуру

По соглашению MNGL HLSL reflection использует `b0, space3` для uniform-блока и `space2` для GPU-ресурсов:

```hlsl
RWTexture2D<float4> output : register(u0, space2);

cbuffer Params : register(b0, space3)
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

mngl::sprite output_sprite{output};
window.clear();
window.draw(output_sprite);
window.display();
```

Конструктор pass, принимающий asset, связывает отражённые ресурсы и перед dispatch отправляет uniform-блок. `dispatch_for()` принимает число потоков шейдера и округляет размеры вверх до целого количества workgroup; шейдер всё равно должен проверять границы dispatch ID. `dispatch(x, y, z)` принимает количество workgroup напрямую. Compute pass завершается при уничтожении или вызове `end_pass()`; завершите его до отправки command buffer.

Для HLSL reflection поддерживает один `cbuffer` в `b0, space3`, а также `Texture2D`, `StructuredBuffer`, `RWTexture2D`, `RWStructuredBuffer` и `SamplerState` в `space2`. Sampled-текстуры связываются с sampler на том же register index. Назначьте поля ресурсов до создания pass. Для sampled-текстуры без явно заданного sampler можно передать default sampler в `compute_pass`.

## GLSL, байткод и явные bindings

`compile_compute()` также принимает GLSL при `source_language = shader_source_language::glsl`: для SPIR-V используется `glslangValidator`, для других форматов — `shadercross`. Размеры из literal-объявления `layout(local_size_x = ..., local_size_y = ..., local_size_z = ...) in;` определяются автоматически. Для макросов и других нелитеральных объявлений задайте `compute_threadgroup_size` явно. `load_compute_binary()` принимает ту же опцию; укажите размер workgroup либо matching source, из которого его можно определить.

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
