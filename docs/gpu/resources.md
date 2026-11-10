# GPU-ресурсы

## Буферы

`mngl::gpu::buffer` — move-only RAII-обёртка над GPU buffer. При создании задаются устройство, размер в байтах и `buffer_usage`. Создание и загрузка через overload с `mngl::gpu::device` удерживают общее состояние устройства до освобождения буфера и его transfer buffer. Overload с `SDL_GPUDevice*` заимствует device, поэтому его время жизни должно покрывать время жизни буфера. `upload()` копирует данные через transfer buffer; источник должен быть trivially copyable, а command buffer — принадлежать тому же устройству.

Для vertex-данных используйте `mngl::gpu::vertex_buffer<T>`. Он принимает range элементов `T`, проверяет совместимость типа и упаковывает несмежные ranges перед загрузкой:

```cpp
std::array<vertex, 3> vertices = /* ... */;
mngl::gpu::command_buffer commands(device);
mngl::gpu::vertex_buffer<vertex> buffer(device, commands, vertices);
commands.submit();
```

Загружайте пустой набор вершин нельзя. Метод `size()` возвращает количество вершин, а `gpu_buffer()` предоставляет лежащий под ним обычный GPU buffer для низкоуровневого binding.

## Текстуры и форматы

`mngl::gpu::texture` создаётся с размером, `gpu::texture_format` и usage-флагами и удерживает состояние переданного `gpu::device` до освобождения текстуры. Например, `sampler` разрешает чтение через sampler, `color_target` — использование как color attachment, `graphics_read` — чтение как graphics storage resource, `compute_read`/`compute_write`/`compute_rw` — доступ из compute pass. Не каждая комбинация формата и usage поддерживается выбранным GPU; создание неподдерживаемой комбинации завершается ошибкой.

`mngl::gpu::texture` не загружает файл и не управляет 2D-кадром. Для повседневной работы используйте [высокоуровневую `mngl::texture`](../2d/render-to-texture.md).

Для compute buffers доступны `compute_storage_read` и `compute_storage_write`; indirect dispatch использует `buffer_usage::indirect`. Эти флаги можно объединять оператором `|`, например `compute_storage_write | indirect` для буфера, который compute shader заполняет командами отрисовки. Права чтения/записи должны совпадать с тем, как ресурс объявлен в compute shader. Подробнее — в [руководстве по compute shaders](compute.md).

## Sampler

`mngl::gpu::sampler` задаёт фильтрацию и адресацию при чтении текстуры. `sampler_builder` позволяет настроить min/mag filters, address mode, mipmap mode, LOD bias, anisotropy и comparison mode. Готовые варианты:

- `create_point_clamp` и `create_point_repeat`;
- `create_linear_clamp` и `create_linear_repeat`.

Sampler и текстура, к которой он применяется, должны принадлежать одному GPU device.

`sampler_builder::build` и фабрики готовых sampler-пресетов принимают как `SDL_GPUDevice*`, так и `mngl::gpu::device`. Вариант с `device` удерживает устройство до освобождения sampler; вариант с `SDL_GPUDevice*` требует внешнего управления временем жизни.

## Диагностика ошибок GPU

Исключения для ошибок SDL включают место вызова и C++ stack trace, если его поддерживает стандартная библиотека. Ошибка создания графического pipeline дополнительно показывает GPU-драйвер, имена vertex/fragment shader, pipeline state, vertex buffers, attributes и color targets. Для compute pipeline выводятся shader, размер threadgroup и раскладка ресурсов. Ошибки shader compiler содержат исходный файл, команду запуска, exit status и вывод процесса; известные ошибки загрузчика Windows, например отсутствующая DLL, поясняются отдельно.

В Debug-сборке `gpu::device` запрашивает GPU debug mode автоматически. Его можно явно включить и в Release-сборке:

```cpp
mngl::gpu::device device{mngl::gpu::device::base_formats, true};
```

Режим debug может включать дополнительную проверку со стороны backend-а и уменьшать скорость. Используйте его для диагностики.
