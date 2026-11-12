# Расширенный 2D-рендеринг

Для обычного рисования используйте [`render_window` и `texture`](index.md). `frame_2d` также доступен внутри callback-отрисовщика, переданного в `draw()`. `mngl::two_d::renderer` и `mngl::gpu::batcher2d` предназначены для интеграции с собственным GPU lifecycle и более низкоуровневых задач.

## Ручное управление кадром

`two_d::renderer::begin_frame(...)` возвращает `two_d::frame_2d`. В кадр можно отправлять `sprite`, стандартные фигуры, `vertex_array` и `convex_polygon`. `frame_2d::finish(command_buffer)` записывает 2D-команды в переданный command buffer, не отправляя его; после этого вызывающий код сам завершает и отправляет буфер.

У `frame_2d` можно задать пользовательский `view` или полностью собственную view-projection matrix. Эти способы взаимоисключающие: последняя установленная проекция определяет итоговую трансформацию кадра.

## Атласы и материалы

`two_d::texture_store` хранит страницы текстур и создаёт `texture_region` по нормализованным UV-границам. Это удобно для набора спрайтов, собранных в атлас. Для анимации по списку регионов используйте `mngl::sprite_animation` (пример — в [руководстве по 2D API](index.md#sprite-atlas-animation)). Для одиночного изображения atlas store не обязателен — передавайте `mngl::texture` напрямую.

`two_d::renderer` также предоставляет создание custom materials и material instances. Материалы используют fragment shader и bindings для uniform blocks, sampled/storage textures и storage buffers. Низкоуровневая настройка этих ресурсов описана в разделе [GPU и шейдеры](../gpu/shaders.md).

Для shader-спрайта renderer создаёт automatic material по снимку bytecode и layout шейдера и сверяет его не чаще одного раза за кадр. Пока renderer использует этот automatic material, `graphics_shader_asset` должен оставаться живым и неизменным, в том числе во время кадра. Изменения между кадрами обнаруживаются при следующей сверке и отклоняются; для новой версии используйте отдельный asset.

`mngl::gpu::batcher2d` — более низкий уровень пакетной отрисовки quad-геометрии. Для пользовательских material instances ресурсы должны принадлежать тому же GPU device и жить не меньше использующих их material instances.

`batcher2d` также позволяет записывать команды в отдельные `recording_context`: создайте контексты во время кадра, записывайте каждый только из одного потока, завершите их через `finish()` и объедините вызовами `merge_recording_context()` в нужном порядке до `flush()`. `flush()` отклоняет кадр с ещё живыми контекстами вместо того, чтобы молча потерять их команды. Чтобы отбросить команды контекста, уничтожьте его; `cancel_frame()` отбрасывает весь кадр.

## Группы, порядок которых не важен

По умолчанию `frame_2d::draw()` сохраняет порядок отрисовки. Если набор объектов можно рисовать в любом порядке, оберните его в `draw_order_independent()`: MNGL отсортирует draw batches этого callback по render state — материалу, текстуре, фильтру и scissor — чтобы сократить переключения GPU-состояния.

```cpp
frame.draw_order_independent([&](auto& group)
{
	for (const auto& icon : icons)
	{
		group.draw(icon);
	}
});
```

Используйте группу только тогда, когда изменение порядка не меняет изображение: например, для неперекрывающихся объектов или непрозрачных объектов без зависимости от слоёв. Обычные прозрачные спрайты при перекрытии обычно зависят от порядка из-за alpha blending. Включённый hit testing также запрещает такие группы, поскольку результат попадания зависит от порядка рисования.

## Отсечение чанков до обхода объектов

Batcher отсекает отдельные draw batches и блоки геометрии при `flush()`. Чтобы не обходить объекты невидимой части большой сцены и не создавать для них draw-команды, разбейте содержимое на чанки и оберните обход каждого чанка в `frame_2d::draw_if_visible()`:

```cpp
for (const auto& chunk : chunks)
{
	frame.draw_if_visible(chunk.bounds, [&](auto& visible_frame)
	{
		chunk.draw(visible_frame); // Обходит объекты только после прохождения AABB-проверки.
	});
}
```

`bounds` — AABB всего содержимого чанка в координатах кадра. Проверка учитывает текущий `view` или `view_projection`; невидимый чанк пропускается до вызова callback. Поэтому в callback размещайте именно обход объектов и подготовку их draw-команд. Для движущихся объектов обновляйте границы чанка; при эффектах и анимации включайте в AABB их максимальный вынос. Несколько уровней чанков позволяют сначала отсечь крупные области, а затем проверять более мелкие. Слишком мелкие чанки увеличивают число AABB-проверок и могут съесть выигрыш.

<a id="2d-optimizations"></a>

## Переключатели оптимизаций 2D

Оптимизационные режимы можно переключать функциями, не меняя обычный цикл `frame.draw()`:

```cpp
#include <MNGL/mngl.hpp>

mngl::gpu::enable_gpu_culling();
mngl::gpu::disable_cpu_culling(); // Например, чтобы отдельно замерить GPU-путь.
mngl::gpu::disable_order_independent_batch_sorting();
mngl::gpu::disable_scene_culling();
mngl::gpu::disable_dirty_region_updates();

mngl::render_window window("Game", {1280, 720});
// Обычные window.frame(), frame.draw() и frame.display() не меняются.
```

Все пять глобальных переключателей находятся в `mngl::gpu`. Если приложению нужны только настройки оптимизаций, достаточно `#include <MNGL/gpu_optimizations.hpp>`; `device.hpp` и общий `mngl.hpp` тоже его включают. Настройки процесса меняются между кадрами; старые `mngl::two_d::enable_scene_culling()` и `mngl::two_d::enable_dirty_region_updates()` оставлены как совместимые aliases. CPU culling, сортировка отмеченных групп, раннее отсечение чанков и dirty-region обновления включены по умолчанию. GPU culling выключен по умолчанию; `enable_gpu_culling()` и `disable_gpu_culling()` влияют на batcher-ы со следующего кадра. Если для конкретного renderer-а нужно исключение, используйте `renderer.enable_gpu_culling()` или `renderer.disable_gpu_culling()`; `renderer.use_global_gpu_culling()` возвращает его к общей настройке.

| Переключатель | Поведение при выключении |
|---|---|
| `mngl::gpu::enable_gpu_culling()` / `mngl::gpu::disable_gpu_culling()` | Включает или выключает compute culling инстансов и indirect culling блоков геометрии. |
| `mngl::gpu::enable_cpu_culling()` / `mngl::gpu::disable_cpu_culling()` | Включает или выключает CPU-отсечение batches и блоков геометрии; без CPU и GPU culling невидимые примитивы дойдут до обычного GPU clipping. |
| `mngl::gpu::enable_order_independent_batch_sorting()` / `mngl::gpu::disable_order_independent_batch_sorting()` | Включает или выключает сортировку только внутри групп, помеченных `draw_order_independent()`. Саму группу по-прежнему нужно отметить в текущем API, чтобы MNGL знал, что порядок менять безопасно. |
| `mngl::gpu::enable_scene_culling()` / `mngl::gpu::disable_scene_culling()` | Включает или выключает проверку AABB в `draw_if_visible()`; при выключении callback выполняется всегда. |
| `mngl::gpu::enable_dirty_region_updates()` / `mngl::gpu::disable_dirty_region_updates()` | Включает частичную перерисовку `cached_layer`; при выключении dirty layer полностью перерисовывается через тот же callback. |

### GPU culling

GPU проверяет каждый sprite и resident mesh instance, а динамическую геометрию — блоками до 64 треугольников. Для инстансов compute shader формирует маску видимости, которую читает vertex shader; порядок инстансов и alpha blending сохраняются. Для динамической геометрии compute shader формирует indirect draw-команды. У полностью невидимых блоков команда получает нулевое число вершин и инстансов, а частично видимые блоки целиком передаются растеризатору, который обрезает невидимые части.

GPU-culling отправляет все исходные инстансы и AABB-метаданные, поэтому объём загрузки выше CPU-пути, когда большинство объектов видимо. Отсечённые инстансы не компактизируются: draw batch и его vertex invocations остаются, но vertex shader проверяет маску до преобразования координат и отправляет скрытые вершины за пределы экрана. Поэтому GPU-путь убирает CPU-проверки видимости, но не уменьшает число instance draw calls и vertex invocations для спрайтов и resident meshes. Для динамической геометрии невидимые indirect-команды имеют нулевое число вершин и инстансов, а видимые блоки отрисовываются как прежде. Сравнивайте CPU-время и ожидание GPU на целевом backend-е.

Стандартные шейдеры MNGL поддерживают маску видимости. При передаче своих `batcher2d_shader_assets` установите `supports_instance_culling = true` только если vertex shader считает NaN в `TEXCOORD4.y` маркером скрытого инстанса и выводит его вершины за пределы clip space. Compute shader записывает этот маркер в instance-буфер только для отсечённых инстансов; при выключенном GPU-culling исходные конечные effect parameters не меняются. Без этого флага включение GPU-culling завершится исключением. Для каждого geometry batch записывается один indirect-вызов с командами его блоков; соседние batch-и не объединяются. Compute shader компилируется при первом включении; приложение должно поставлять каталог `shaders/` и инструменты шейдеров для выбранного формата GPU так же, как для остальных runtime-компилируемых HLSL шейдеров.

Compute и multi-draw indirect добавляют проход и загрузку метаданных, поэтому на небольших кадрах режим может быть медленнее. Включайте его после профилирования сцены с большим количеством геометрии и сравнивайте `last_frame_stats()`.

`mesh`, `prepared_vertex_array` и `cached_layer` остаются явными ресурсами текущего 2D API: они сообщают MNGL, что данные подготовлены, постоянны или могут кэшироваться. Глобальные переключатели меняют стратегии рендерера и обновления этих ресурсов, но не выводят статичность из произвольных изменяемых объектов. Низкоуровневые fast paths вроде повторного использования GPU bindings и объединённой загрузки остаются внутренней реализацией.

## Профилирование кадра

Замеры включаются отдельно у renderer и не выполняются по умолчанию:

```cpp
auto& renderer = window.renderer();
renderer.enable_profiling();

// В игровом цикле после frame.display():
const auto& stats = renderer.last_frame_stats();
const double flush_ms = std::chrono::duration<double, std::milli>(stats.flush_cpu_time).count();
```

Поля времени в `frame_stats` содержат CPU-время и равны нулю для кадров, записанных с выключенным профилированием:

- `frame_recording_wall_time` — wall time от `begin_frame()` до начала `flush()`; в него входит работа вызывающего кода и ожидание завершения параллельной записи.
- `culling_cpu_time` — CPU-время отсечения и подготовки метаданных для GPU-culling.
- `upload_cpu_time` — подготовка staging-данных и запись copy-команд.
- `command_encoding_cpu_time` — запись compute/render-команд на стороне CPU.
- `flush_cpu_time` — всё CPU-время внутри batcher `flush()`, без отправки command buffer.

SDL GPU в используемой MNGL версии не предоставляет переносимых timestamp-запросов для времени исполнения GPU-команд. Эти счётчики не измеряют выполнение GPU. Время `frame()`, вызовов `frame.draw()` и `frame.display()` измеряйте на стороне приложения; benchmark из репозитория выводит эти интервалы отдельно. Для повторяемых замеров используйте Release-сборку и исключайте первые кадры после переключения сцены или режима.

В репозитории есть интерактивная сцена `mngl_2d_benchmark`; её сборка и клавиши описаны в [руководстве по benchmark](benchmarking.md).

