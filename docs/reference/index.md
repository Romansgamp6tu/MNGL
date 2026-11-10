# Карта публичных модулей

Здесь перечислены основные заголовки MNGL и руководство по соответствующему модулю. Встроенные заголовки `include/SDL3`, `include/SDL3_image` и `include/glm` — сторонние библиотеки; их документация здесь не дублируется.

Подключение к стороннему проекту описано в разделе [Использование MNGL](../getting-started/using-mngl.md),
сборка самого проекта — в разделе [Сборка и пакеты](../getting-started/building.md).
В development-пакете `main` заголовки ниже доступны с префиксом `MNGL/`,
например `<MNGL/audio.hpp>`; CMake target также поддерживает короткие include paths.

| Заголовки | Назначение | Руководство |
|---|---|---|
| `mngl.hpp` | Необязательный umbrella-заголовок для обычного приложения: окна, ввод, 2D и его настройки, аудио, файлы и платформенные сервисы | [Подключение MNGL](../getting-started/using-mngl.md) |
| `window.hpp`, `input.hpp` | Окна, дисплеи, отдельные устройства и состояния ввода | [Окна и ввод](../core/windows-and-input.md) |
| `action_map.hpp` | Привязки действий, контексты и пресеты ввода | [Действия и контексты](../core/windows-and-input.md#actions-and-contexts) |
| `audio.hpp` | `sound`, потоковая `music`, capture, живой PCM и пространственные голоса | [Аудио](../core/audio.md) |
| `dialog.hpp` | Системные диалоги выбора файлов и папок | [Системные диалоги](../core/dialogs.md) |
| `clipboard.hpp` | Текстовый и MIME-буфер обмена, отложенная выдача данных | [Буфер обмена](../core/clipboard.md) |
| `storage.hpp` | RAII-хранилища приложения, пользователя и локального каталога | [Хранилище и файлы](../core/storage.md) |
| `async_io.hpp` | Фоновый ввод-вывод с типизированным результатом и ожиданием запроса | [Асинхронный ввод-вывод](../core/async-io.md) |
| `process.hpp` | Запуск процессов, pipe-ввод с `close_stdin()` для EOF и получение кода завершения | [Процессы](../core/processes.md) |
| `paths.hpp` | Каталоги приложения и стандартные папки пользователя | [Runtime и утилиты](../core/runtime-and-utilities.md) |
| `imgui_compat.hpp` | MNGL scopes и классы-виджеты поверх Dear ImGui; `button::is_small()`, меню, canvas, ввод, GPU overlay и шрифты | [Окна и ввод](../core/windows-and-input.md) |
| `event.hpp` | Типизированная система событий | [События](../core/events.md) |
| `surface.hpp`, `pixel_format.hpp` | SDL surfaces, изображения и форматы пикселей | [Поверхности и цвета](../core/media-and-colors.md) |
| `color.hpp` | Цвета, преобразования RGB/HSV/HSL | [Поверхности и цвета](../core/media-and-colors.md) |
| `line.hpp`, `rectangle.hpp` | Геометрические примитивы и пересечения | [Геометрия](../core/geometry.md) |
| `utilities.hpp`, `paths.hpp`, `logger.hpp` (`logger.h` для старых глобальных имён), `iupdate.h`, `proxy.hpp` | Runtime, идентичность приложения, пути, ошибки, logging, обновления и SDL interop | [Runtime и утилиты](../core/runtime-and-utilities.md) |
| `console.hpp` | Реестр типизированных CLI-команд, режимы вывода и результатов обработчиков | [Консольные команды](../core/console.md) |
| `gpu_optimizations.hpp` | Общие функции включения и выключения GPU culling, CPU culling, сортировки, scene culling и обновления dirty regions | [Переключатели оптимизаций 2D](../2d/advanced-rendering.md#2d-optimizations) |
| `device.hpp`, `command_buffer.hpp` | Разделяемое время жизни GPU device и явная отправка команд | [Устройство и команды](../gpu/device-and-commands.md) |
| `gpu_buffer.hpp`, `gpu_texture.hpp`, `sampler.hpp` | Низкоуровневые GPU-ресурсы с RAII-владением | [GPU-ресурсы](../gpu/resources.md) |
| `graphics_shader.hpp`, `shader_loader.hpp` | Загрузка/компиляция шейдеров, uniforms и bindings | [Шейдеры](../gpu/shaders.md) |
| `vertex_layout.hpp`, `graphics_pipeline.hpp`, `render_pass.hpp` | Vertex input и графические проходы | [Pipeline и render pass](../gpu/pipelines-and-passes.md) |
| `render_window.hpp`, `renderer2d.hpp`, `texture.hpp`, `view.hpp`, `transform.hpp`, `hit_test.hpp` | Высокоуровневый 2D API, включая геометрию, GPU mesh, кэшированные слои и переключатели рендеринга | [Раздел 2D](../2d/index.md), [переключатели оптимизаций 2D](../2d/advanced-rendering.md#2d-optimizations) |
| `batcher2d.hpp` | Низкоуровневая пакетная 2D-отрисовка, GPU culling и CPU-профилирование кадров | [Расширенный 2D-рендеринг](../2d/advanced-rendering.md) |
| `text.hpp` | Шрифты, текст, текстовые документы и атлас глифов | [Рендер текста](../2d/text-rendering.md) |
| `mods.hpp`, `mod_loader.hpp` | Точки расширения и загрузка нативных модов | [Контракт модов](../mods/api-contract.md) |

`detail/audio/sdl3_mixer_backend.hpp` — внутренняя реализация аудио; приложению
достаточно `audio.hpp`. Конфигурации с отключёнными слоями могут исключать
заголовки и интерфейсы, см. [настройки аудио](../core/audio.md#audio-configuration).

Руководства объясняют главные сценарии и ограничения. Полный symbol-by-symbol справочник по объявлениям C++ пока не генерируется; его можно добавить отдельным Doxygen-выводом, не меняя исходники страниц MkDocs.

`event.hpp`, `input.hpp`, `action_map.hpp`, `console.hpp`, `imgui_compat.hpp`, `batcher2d.hpp`,
`renderer2d.hpp`, `shader_loader.hpp`, `mods.hpp` и `mod_loader.hpp` остаются публичными точками
входа. Их реализация разделена внутри `MNGL/detail/`; подключайте публичные фасады, а не внутренние
файлы.

## Базовые и дополнительные заголовки

- `error.hpp` — `mngl::sdl_error` и `check_sdl` без тяжёлых зависимостей.
- `runtime.hpp` — инициализация SDL и `runtime::ensure()`.
- `file_io.hpp` — синхронное чтение файла без графического слоя.
- `extras/tree_renderer.hpp` — явно подключаемая визуализация структур деревьев;
  старый `tree_renderer.hpp` перенаправляет include для совместимости.

Соглашения для новых модулей: [Принципы и стиль API](../design/api-style.md).
