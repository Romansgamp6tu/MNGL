# Vertex layout, pipeline и render pass

## Описание вершин

`mngl::gpu::vertex_layout` связывает shader locations с входными буферами. Для каждого buffer задаются slot, stride и частота подачи (`vertex` либо `instance`); для каждого атрибута — location, slot, формат и byte offset.

Если структура вершины trivially copyable, stride можно вывести из её размера:

```cpp
mngl::gpu::vertex_layout layout;
layout.add_buffer<vertex>(0)
    .add_attribute(
        0,
        0,
        mngl::gpu::vertex_elemetn_fmt::float2,
        offsetof(vertex, position));
```

Атрибуты в shader должны использовать те же locations и совместимые форматы.

## Graphics pipeline

`mngl::gpu::graphics_pipeline_builder` настраивает vertex/fragment shaders, vertex layout, тип примитивов, rasterizer, depth state и color targets. Для построения обязательны оба shader stage. Готовый `graphics_pipeline` — move-only GPU resource:

```cpp
auto pipeline = mngl::gpu::graphics_pipeline_builder{}
    .set_vertex_shader(vertex_shader)
    .set_fragment_shader(fragment_shader)
    .set_vertex_layout(layout)
    .add_alpha_blend_target(
        static_cast<SDL_GPUTextureFormat>(target.format()))
    .build(device.raw());
```

Pipeline должен быть создан для формата целевого color attachment, с которым он будет использоваться.

## Render pass

`mngl::gpu::render_pass` открывает проход записи команд в `command_buffer`. Простой overload принимает одну color texture; универсальный принимает список до четырёх color attachments и необязательный depth-stencil attachment. Для каждой цели можно выбрать load/store operation; по умолчанию цвет очищается и сохраняется.

Внутри прохода pipeline и буферы связываются перед draw-командами. Uniform-данные передаются отдельно для vertex и fragment stage, а draw API поддерживает обычные и индексированные примитивы. Деструктор render pass завершает активный проход, поэтому он должен быть уничтожен до `command_buffer::submit()`.
