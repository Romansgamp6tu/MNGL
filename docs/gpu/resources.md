# GPU-ресурсы

## Буферы

`mngl::gpu::buffer` — move-only RAII-обёртка над GPU buffer. При создании задаются устройство, размер в байтах и `buffer_usage`. `upload()` копирует данные через transfer buffer; источник должен быть trivially copyable, а command buffer — принадлежать тому же устройству.

Для vertex-данных используйте `mngl::gpu::vertex_buffer<T>`. Он принимает range элементов `T`, проверяет совместимость типа и упаковывает несмежные ranges перед загрузкой:

```cpp
std::array<vertex, 3> vertices = /* ... */;
mngl::gpu::command_buffer commands(device);
mngl::gpu::vertex_buffer<vertex> buffer(device, commands, vertices);
commands.submit();
```

Загружайте пустой набор вершин нельзя. Метод `size()` возвращает количество вершин, а `gpu_buffer()` предоставляет лежащий под ним обычный GPU buffer для низкоуровневого binding.

## Текстуры и форматы

`mngl::gpu::texture` создаётся с размером, `gpu::texture_format` и usage-флагами. Например, `sampler` разрешает чтение через sampler, `color_target` — использование как color attachment, `graphics_read` — чтение как graphics storage resource. Не каждая комбинация формата и usage поддерживается выбранным GPU; создание неподдерживаемой комбинации завершается ошибкой.

`mngl::gpu::texture` не загружает файл и не управляет 2D-кадром. Для повседневной работы используйте [высокоуровневую `mngl::texture`](../2d/render-to-texture.md).

## Sampler

`mngl::gpu::sampler` задаёт фильтрацию и адресацию при чтении текстуры. `sampler_builder` позволяет настроить min/mag filters, address mode, mipmap mode, LOD bias, anisotropy и comparison mode. Готовые варианты:

- `create_point_clamp` и `create_point_repeat`;
- `create_linear_clamp` и `create_linear_repeat`.

Sampler и текстура, к которой он применяется, должны принадлежать одному GPU device.
