# Карта публичных модулей

Здесь перечислены основные заголовки MNGL и руководство по соответствующему модулю. Встроенные заголовки `include/SDL3`, `include/SDL3_image` и `include/glm` — сторонние библиотеки; их документация здесь не дублируется.

Текущий процесс сборки и статус подключения к стороннему проекту описаны в разделе [Сборка и подключение](../getting-started/building.md).

| Заголовки | Назначение | Руководство |
|---|---|---|
| `window.hpp`, `input.hpp` | Окна, дисплеи, ввод | [Окна и ввод](../core/windows-and-input.md) |
| `imgui_compat.hpp` | Header-only обёртка Dear ImGui, GPU overlay и настройка шрифтов | [Окна и ввод](../core/windows-and-input.md) |
| `event.hpp` | Типизированная система событий | [События](../core/events.md) |
| `surface.hpp`, `pixel_format.hpp` | SDL surfaces, изображения и форматы пикселей | [Поверхности и цвета](../core/media-and-colors.md) |
| `color.hpp` | Цвета, преобразования RGB/HSV/HSL | [Поверхности и цвета](../core/media-and-colors.md) |
| `line.hpp`, `rectangle.hpp` | Геометрические примитивы и пересечения | [Геометрия](../core/geometry.md) |
| `utilities.hpp`, `logger.h`, `iupdate.h`, `proxy.hpp` | Runtime, ошибки, logging, обновления и SDL interop | [Runtime и утилиты](../core/runtime-and-utilities.md) |
| `console.hpp` | Реестр типизированных CLI-команд, режимы вывода и результатов обработчиков | [Консольные команды](../core/console.md) |
| `device.hpp`, `command_buffer.hpp` | GPU device и command buffers | [Устройство и команды](../gpu/device-and-commands.md) |
| `gpu_buffer.hpp`, `gpu_texture.hpp`, `sampler.hpp` | Низкоуровневые GPU-ресурсы | [GPU-ресурсы](../gpu/resources.md) |
| `graphics_shader.hpp`, `shader_loader.hpp` | Загрузка/компиляция шейдеров, uniforms и bindings | [Шейдеры](../gpu/shaders.md) |
| `vertex_layout.hpp`, `graphics_pipeline.hpp`, `render_pass.hpp` | Vertex input и графические проходы | [Pipeline и render pass](../gpu/pipelines-and-passes.md) |
| `render_window.hpp`, `renderer2d.hpp`, `texture.hpp`, `view.hpp`, `transform.hpp`, `hit_test.hpp` | Высокоуровневый 2D API | [Раздел 2D](../2d/index.md) |
| `batcher2d.hpp` | Низкоуровневая пакетная 2D-отрисовка | [Расширенный 2D-рендеринг](../2d/advanced-rendering.md) |

Руководства объясняют главные сценарии и ограничения. Полный symbol-by-symbol справочник по объявлениям C++ пока не генерируется; его можно добавить отдельным Doxygen-выводом, не меняя исходники страниц MkDocs.
