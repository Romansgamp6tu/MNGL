# Низкоуровневый GPU API

Простое 2D-рисование описано в [разделе 2D](../2d/index.md). `mngl::gpu` — более низкоуровневый интерфейс к SDL GPU: он предоставляет явное управление устройством, командными буферами, ресурсами и графическими проходами.

Типичный GPU-путь состоит из следующих шагов:

1. Получить `mngl::gpu::device` и создать ресурсы на этом устройстве.
2. Подготовить graphics- или compute-шейдер и соответствующий pipeline.
3. Записать команды в `command_buffer`, открыв `render_pass` или `compute_pass`.
4. Завершить проход и отправить команды через `command_buffer::submit()`.

Начните с [устройства и команд](device-and-commands.md), затем изучите [ресурсы](resources.md), [шейдеры](shaders.md), [compute shaders](compute.md), [pipeline/render pass](pipelines-and-passes.md) и [граф кадра](frame-graph.md).

`mngl::gpu::texture` — низкоуровневая GPU-текстура. Она отличается от высокоуровневой `mngl::texture`, которая умеет загружать пиксели и предоставляет простой 2D render lifecycle.
