# Устройство и командные буферы

## Устройство и его владельцы

`mngl::gpu::device` — копируемая ручка на общее состояние SDL GPU device. Копии `device`, ресурсы и command buffers, созданные через перегрузки с `device`, удерживают его живым. SDL device уничтожается после освобождения последнего владельца.

Обычное приложение использует общий экземпляр:

```cpp
#include <MNGL/device.hpp>

const auto& device = mngl::gpu::device::global();
```

`render_window` и обычные конструкторы `mngl::texture` используют глобальный render context. Если создаёте собственное устройство, передавайте его явно во все ресурсы соответствующего GPU-пути. Наличие двух переменных `device` ещё не означает наличие двух устройств: копии разделяют владение; отдельный вызов конструктора создаёт новое устройство.

Перегрузки с `SDL_GPUDevice*` заимствуют указатель. В этом случае вызывающий отвечает за то, чтобы SDL device пережил все созданные через него обёртки. `raw()` также возвращает заимствованный handle.

## Первый GPU-пример без shader tools

Программа создаёт текстуру 256 × 256, очищает её цветом и отправляет команды. Она не открывает окно: результат находится в offscreen-текстуре. Этот шаг позволяет проверить создание device и запись прохода отдельно от загрузки шейдеров.

```cpp
#include <MNGL/command_buffer.hpp>
#include <MNGL/device.hpp>
#include <MNGL/gpu_texture.hpp>
#include <MNGL/render_pass.hpp>
#include <MNGL/runtime.hpp>

#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::runtime::ensure();
		const auto& device = mngl::gpu::device::global();
		using usage = mngl::gpu::texture::usage_flags;

		mngl::gpu::texture target(
		    device, {256, 256}, mngl::gpu::texture_format::r8g8b8a8_unorm,
		    usage::color_target | usage::sampler);

		mngl::gpu::command_buffer commands(device);
		{
			mngl::gpu::render_pass pass(
			    commands, target, mngl::color{0.1f, 0.3f, 0.7f, 1.0f});
		} // Деструктор завершает render pass.

		commands.submit();
		device.wait(); // Здесь ожидаем завершения для проверки примера.
		std::cout << "GPU clear completed\n";
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
```

`color_target` разрешает использовать текстуру как выход render pass. `sampler` разрешает прочитать результат в следующем графическом проходе. Цвет очищается при начале прохода; draw-команды для этого не требуются.

`wait()` возвращает `void` и бросает исключение при ошибке. `try_wait()` возвращает `std::expected<void, sdl_error>`. Ожидание всего устройства удобно для диагностики или завершения работы; вызов каждый кадр заставляет CPU ждать GPU и уменьшает параллелизм.

## Состояния command buffer

`command_buffer commands(device)` приобретает буфер для записи. Пока он активен, в него можно записывать загрузки и последовательные проходы. Завершите его одним из способов:

| Действие | Что произойдёт |
|---|---|
| `submit()` | Записанная работа отправится в GPU-очередь |
| `cancel()` | Записанная работа будет отброшена |
| Уничтожение активного объекта | Деструктор отменит работу без выброса исключений |

После `submit()` или `cancel()` объект больше не содержит активного SDL command buffer. Для следующей порции работы создайте новый. Отправка означает постановку в очередь; завершение CPU-вызова `submit()` не означает завершения всех GPU-команд.

RAII позволяет безопасно прервать подготовку кадра исключением:

```cpp
void upload_values(const mngl::gpu::device& device,
                   mngl::gpu::buffer& destination,
                   std::span<const float> values)
{
	mngl::gpu::command_buffer commands(device);
	destination.upload(device, commands, values);
	// Если upload() бросит исключение, commands отменится при выходе.
	commands.submit();
}
```

Для этого фрагмента нужны `<MNGL/gpu_buffer.hpp>` и `<span>`. Функция принимает уже созданный буфер того же устройства. Она не глотает исключение: вызывающий может показать ошибку пользователю или завершить загрузку.

## Проходы идут последовательно

В одном command buffer можно сначала загрузить данные, затем выполнить compute и render. Сначала завершайте предыдущий проход, после этого начинайте следующий. Не вызывайте загрузку буфера, пока открыт render pass: `upload()` сам открывает copy pass. Удобнее разделять их областями видимости:

```cpp
mngl::gpu::command_buffer commands(device);
vertices.upload(device, commands, cpu_vertices);
{
	mngl::gpu::render_pass pass(commands, target, clear_color);
	pass.bind_pipeline(pipeline);
	pass.bind_vertex_buffer(0, vertices);
	pass.draw_primitives(vertices.size());
}
commands.submit();
```

Здесь `vertices` — `vertex_buffer<T>`, `cpu_vertices` — range вершин того же типа, а `target`, `pipeline` и `clear_color` созданы заранее. [Пример их согласования](pipelines-and-passes.md) разбирает layout и форматы.

## Потоки, окна и swapchain

Command buffer принадлежит потоку, на котором приобретён. Использовать, перемещать и уничтожать его нужно на том же потоке. Вызов методов из другого потока приводит к `std::logic_error`; деструктор на чужом потоке пишет ошибку в SDL log и не может отменить буфер.

Ручной оконный путь дополнительно требует `device.claim_window(window)`, получения `commands.acquire_swapchain_texture(window)` и освобождения окна через `release_window()`. Получение swapchain происходит на главном потоке окна. При минимизации SDL может не дать пригодную для рисования текстуру: проверяйте `valid()` перед открытием прохода. Полученная текстура заимствует изображение swapchain и относится к текущему кадру; её нельзя сохранять как постоянный render target.

Для обычного 2D-приложения используйте `render_window::set_swapchain_parameters(hdr, vsync)`:
метод задаёт режим композиции HDR/SDR и синхронизацию между кадрами, а
`swapchain_texture_format()` сообщает формат, который выбрал SDL/GPU backend.
Произвольный формат swapchain не назначается напрямую. Переданный в
`render_window::create_texture(size)` формат по умолчанию берётся из текущего
swapchain; явная перегрузка позволяет создать offscreen-цель другого формата.
Практический фрагмент находится в [быстром старте 2D](../2d/quick-start.md).

Для обычной 2D-отрисовки `render_window` выполняет claim/release и оконный lifecycle. Не начинайте параллельно собственный ручной кадр для того же окна.

## Диагностика устройства

Debug-сборка запрашивает GPU debug mode автоматически. Для отдельного устройства его можно включить явно:

```cpp
mngl::gpu::device debug_device{mngl::gpu::device::base_formats, true};
```

Если создание устройства не удалось, сначала сохраните полный текст исключения и имя backend-а из SDL log. Если ошибка возникает только при создании ресурса, проверьте его формат и usage. Если только при построении pipeline — формат shader bytecode, layout и формат render target.

