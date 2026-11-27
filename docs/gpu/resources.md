# GPU-ресурсы

Ресурсы описывают данные, с которыми работает GPU. Буфер хранит байты, текстура — изображение заданного формата, sampler — правила чтения текстуры. Их создание и загрузка обычно происходят до игрового цикла; в кадре меняются только необходимые данные.

## Владение и устройство

`gpu::buffer`, `gpu::texture` и `gpu::sampler` — move-only RAII-объекты. Их нельзя копировать; передавайте ссылки или перемещайте владельца. Перегрузки с `gpu::device` удерживают его общее состояние. Низкоуровневые перегрузки с `SDL_GPUDevice*` требуют, чтобы вызывающий сам сохранял device живым.

Ресурс, command buffer и использующий его pipeline должны относиться к одному GPU device. Держите ресурсы живыми до отправки записанных с ними команд. Передача `raw()` в SDL не передаёт владение и не создаёт дополнительного владельца.

## Первый массив вершин

Для вершин удобен `vertex_buffer<T>`: он хранит количество элементов и создаёт GPU buffer с нужным vertex usage. Ниже законченная программа загрузки. Она не рисует изображение; [следующий раздел](pipelines-and-passes.md) добавляет layout, shaders и draw-команду.

```cpp
#include <MNGL/device.hpp>
#include <MNGL/gpu_buffer.hpp>
#include <MNGL/runtime.hpp>

#include <array>
#include <exception>
#include <iostream>

struct vertex
{
	float x;
	float y;
};

int main()
{
	try
	{
		mngl::runtime::ensure();
		const auto& device = mngl::gpu::device::global();
		const std::array<vertex, 3> triangle{{
		    {-0.5f, -0.5f},
		    { 0.5f, -0.5f},
		    { 0.0f,  0.5f},
		}};

		mngl::gpu::command_buffer commands(device);
		mngl::gpu::vertex_buffer<vertex> vertices(device, commands, triangle);
		commands.submit();
		device.wait();

		std::cout << vertices.size() << " vertices uploaded\n";
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
```

Тип вершины должен быть trivially copyable. Конструктор принимает range элементов именно этого типа; пустой range вызывает `std::invalid_argument`. Для несмежного range данные сначала упаковываются на CPU. Загрузка копирует данные в transfer buffer до возвращения из вызова, поэтому исходный CPU-массив не обязан жить до `submit()`.

Важно различать единицы измерения:

| Метод | Единица |
|---|---|
| `vertex_buffer<T>::size()` | Количество вершин |
| `buffer::size()` | Выделенный размер GPU buffer в байтах |
| `render_pass::bind_vertex_buffer(..., offset)` | Смещение в байтах |
| `buffer::upload_range(..., first_element, span<T>)` | Индекс элемента типа `T` |
| `draw_primitives(num_vertices)` | Количество вершин |

GPU buffer может выделять запас ёмкости; не выводите число вершин из `gpu_buffer().size()`. Для draw используйте `vertices.size()`.

## Обновление одного участка

Чтобы заменить вторую вершину, не изменяя остальные, используйте `upload_range()`. В этом фрагменте `vertices` и `device` сохранены из предыдущего примера; новый command buffer нужен для новой отправки:

```cpp
#include <span>

const std::array<vertex, 1> replacement{{{0.75f, -0.25f}}};
mngl::gpu::command_buffer update(device);

vertices.gpu_buffer().upload_range(
    device, update, 1, std::span<const vertex>(replacement));
update.submit();
```

`1` — индекс второй вершины, а не байтовое смещение. Внутри `upload_range()` MNGL копирует содержимое GPU buffer в запасной буфер и загружает только изменённый участок с CPU. Это сохраняет байты вне участка и данные для ранее записанных команд. Полное GPU-копирование всё равно имеет стоимость: для буфера, который целиком меняется каждый кадр, выбирайте полную `upload()`.

После полной загрузки или частичного обновления `raw()` может вернуть другой SDL handle. Получайте handle для binding после обновления; не кешируйте его как постоянную идентичность ресурса. Диапазон за пределами выделенного буфера приводит к `std::out_of_range`.

<a id="regular-buffer-usage"></a>

## Обычный buffer и usage

Для индексов, storage и indirect-данных используйте `gpu::buffer`. Тип данных задаётся при загрузке, usage — при создании:

```cpp
#include <MNGL/gpu_buffer.hpp>
#include <array>
#include <cstdint>
#include <span>

const std::array<std::uint32_t, 3> indices{0, 1, 2};
mngl::gpu::command_buffer commands(device);
auto index_buffer = mngl::gpu::buffer::create_with_data(
    device, commands, std::span<const std::uint32_t>(indices),
    mngl::gpu::buffer_usage::index);
commands.submit();
```

`device` здесь — уже созданное устройство. Индексы в примере 32-битные: при binding выбирайте `SDL_GPU_INDEXELEMENTSIZE_32BIT`. По умолчанию `render_pass::bind_index_buffer()` ожидает 16-битные индексы.

Для storage buffer выберите `graphics_storage_read`, `compute_storage_read` или `compute_storage_write` по фактическому использованию. Флаги объединяются через `|`. Например, compute-шейдер может записать indirect-команды в буфер с `compute_storage_write | indirect`. О том, где связывать writable и read-only ресурсы, см. [Compute shaders](compute.md).

## Текстуры: размер, формат и допустимые операции

`gpu::texture` создаётся с размером, `gpu::texture_format` и usage-флагами:

```cpp
#include <MNGL/gpu_texture.hpp>

using usage = mngl::gpu::texture::usage_flags;
mngl::gpu::texture scene(
    device, {1024, 1024}, mngl::gpu::texture_format::r8g8b8a8_unorm,
    usage::color_target | usage::sampler);
```

Эту текстуру можно использовать как выход графического прохода и затем прочитать через sampler. Она содержит выделенную GPU-память; загрузки PNG и начального цвета этот конструктор не выполняет. До чтения задайте содержимое загрузкой или проходом записи.

| Usage | Для чего нужен |
|---|---|
| `sampler` | Чтение изображения через sampler |
| `color_target` | Color attachment render pass |
| `stencil_target` | Depth/stencil attachment; нужен подходящий depth-формат |
| `graphics_read` | Read-only storage texture в graphics shader |
| `compute_read` | Read-only storage texture в compute shader |
| `compute_write` | Запись compute shader |
| `compute_rw` | Одновременное чтение и запись compute shader |

Имя `stencil_target` в таблице — фактическое имя флага в текущем API. Не каждая комбинация usage и формата поддерживается backend-ом; конструктор проверяет её и сообщает `std::invalid_argument` при несовместимости. Размеры должны быть ненулевыми.

`has_usage(a | b)` возвращает `true`, только когда присутствуют оба флага. Для проверки любого используйте `has_usage(a) || has_usage(b)`.

Для PNG, JPEG и обычного 2D-кадра используйте `mngl::texture`. Она предоставляет `raw_gpu_texture()` для GPU-прохода и затем может отображаться через `mngl::sprite`; пример такого сочетания есть в [compute-руководстве](compute.md). Это сохраняет простой 2D lifecycle, когда собственное управление загрузкой пикселей не требуется.

### Формат изображения для 2D и спрайтов

По умолчанию загрузка файла, `SDL_Surface` или `mngl::surface` создаёт
`r8g8b8a8_unorm`. Выберите формат вторым аргументом, когда важна sRGB
интерпретация или порядок каналов:

```cpp
#include <MNGL/texture.hpp>
#include <MNGL/renderer2d.hpp>

auto& textures = window.textures();
const auto color_image = textures.add(mngl::texture(
    "assets/color.png", mngl::gpu::texture_format::r8g8b8a8_unorm_srgb));
const auto bgra_image = textures.add(mngl::texture(
    "assets/ui.png", mngl::gpu::texture_format::b8g8r8a8_unorm));
mngl::sprite ui{textures, bgra_image};

const auto format = textures.resolve(bgra_image).format();
```

У `sprite` нет отдельного формата пикселей: он наследуется от исходной
текстуры, на которую указывает handle. То же относится к `texture_region` в
texture store и `sprite_animation` — выберите формат до добавления текстуры.
`texture::format()` возвращает фактический GPU-формат.

Загрузка обычных изображений из surface поддерживает четыре формата:
`r8g8b8a8_unorm`, `r8g8b8a8_unorm_srgb`, `b8g8r8a8_unorm` и
`b8g8r8a8_unorm_srgb`. Другие GPU-форматы можно создать низкоуровневым
`gpu::texture` или передать в конструктор `mngl::texture` уже согласованные
сырые пиксельные данные. PNG-декодер не выполняет автоматическое преобразование
в float, integer или compressed block formats. GPU также должен поддерживать
выбранное сочетание формата и usage.

`animated_texture(path, format)` сохраняет выбранный формат при загрузке
последующих кадров GIF. Для пустой offscreen-цели применяйте формат напрямую:
`window.create_texture(size, format)`. По умолчанию этот метод наследует формат
swapchain окна.

Формат swapchain не задаётся произвольным enum: его выбирает SDL/GPU backend.
`render_window::set_swapchain_parameters(hdr_mode, vsync_mode)` позволяет
попросить SDR/HDR-композицию и способ синхронизации, а
`render_window::swapchain_texture_format()` сообщает фактически выбранный
формат. Изменяйте параметры между кадрами; backend может отклонить режим,
который не поддерживается на текущей системе. Полный пример есть в
[быстром старте](../2d/quick-start.md).

## Sampler: пиксельная графика и плавное масштабирование

Sampler отвечает за выбор соседних texels и поведение за краем текстуры. Для pixel art обычно нужен nearest, а для плавного увеличения — linear. Clamp останавливает координату у границы, repeat повторяет изображение.

```cpp
#include <MNGL/sampler.hpp>

auto pixels = mngl::gpu::sampler_builder::create_point_clamp(device);
auto smooth = mngl::gpu::sampler_builder::create_linear_clamp(device);
auto tiled = mngl::gpu::sampler_builder::create_linear_repeat(device);
```

Эти фабрики находятся в `sampler_builder` и принимают `device` или `SDL_GPUDevice*`. Более подробные настройки builder-а включают min/mag filter, address mode, mipmap mode, LOD bias, anisotropy и comparison mode. Sampler и текстура должны относиться к одному устройству.

## Что проверять при ошибке ресурса

Если ошибка появляется при создании текстуры, начните с размера, формата и usage. Если при upload — проверьте устройство command buffer, тип элементов и размер данных. Если изображение выглядит искажённым при корректной загрузке — проверьте vertex layout и shader inputs.

SDL exceptions включают место вызова и диагностический контекст; pipeline-ошибки дополнительно показывают состояние и имена shaders. Сохраните полный текст исключения, а не только первую строку. Debug mode устройства включается автоматически в Debug-сборке; ручное включение показано в [устройствах и командах](device-and-commands.md#device-diagnostics).

