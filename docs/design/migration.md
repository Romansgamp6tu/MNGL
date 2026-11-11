# Миграция на единый C++23 API

`color::rgb_uint()` исправлен на `0xRRGGBB`; `rgba_uint()` сохраняет `0xRRGGBBAA`.
HEX-конструктор отвергает частично распознанные строки, пробелы, знак и `0x`.
У `rectangle` касание границ считается пересечением, как у SDL float rectangles;
пустые области не расширяют union. `enclosing_points(points, clip)` игнорирует
точки за пределами clip, вместо пересечения общего bounding box с clip.
Методы `render_pass::push_vertex_uniform()` и `push_fragment_uniform()` больше
не объявлены `noexcept`: пустой, завершённый или перемещённый проход сообщает
`std::logic_error`. Данные передаются в SDL command buffer, а не в render-pass handle.

`two_d::text_draw_options::wrap_width` задаёт автоматический перенос строкового
`text` в координатах сцены; `0.0f` его отключает. Поле добавлено в конец
options-структуры, поэтому прежние агрегатные инициализаторы сохраняют порядок.
`text_document` остаётся построчным и не принимает ненулевую ширину переноса.

`action_map::save_profile(path)` и `load_profile(path)` сохраняют привязки в
версионированном таб-разделённом файле MNGL. Загрузка проверяет весь профиль до
замены общей карты; уже созданные `input_context` получают новые привязки. Файлы
профиля не сохраняют состояние контекста.

`process::child_process::close_stdin()` закрывает pipe-ввод и отправляет EOF:
вызывайте его после последнего `write_stdin()`, если процесс читает до конца ввода.
У ImGui-виджета `button` используйте getter `is_small()` вместе с `set_small()`.
Совместимый `small()` сохранён, но Windows RPC headers могут определять одноимённый макрос.
`gpu::texture::has_usage(a | b)` теперь требует оба флага; для проверки любого
из них используйте `has_usage(a) || has_usage(b)`.

Новые примеры используют `snake_case`. Для старых имён типов сохранены aliases:
`Input → input`, `ActionMap → action_map`, `InputContext → input_context`,
`Frame2D → frame_2d`, `Process → child_process`, `Storage → store`,
`TitleStorage → title_storage`, `UserStorage → user_storage`, `FileStorage → file_storage`,
`async_io::Queue → queue`, `async_io::Request → request`.

Для синхронного сценария поверх async I/O используйте `queue::wait(request)`: он возвращает
`request_outcome` со статусными метаданными и данными чтения в одном `std::expected`.
Старый `queue::wait(timeout)` и snapshot `request::result()` сохраняются для циклов опроса.

В аудио v1: `Sound → sound`, `Music → music`, `VoiceHandle → voice_handle`,
`PlayOptions → play_options`, `Bus → bus`, `AudioErrorCode → error_code`, `AudioError → error`.
В аудио v2 добавлены `RecordingDevice`, `CaptureOptions`, `CaptureSession`,
`AudioStreamFormat`, `AudioStream` и `Position3D`; доступны их snake_case имена.
Старые вызовы `play_options` остаются совместимыми: поле `position` добавлено
после существующих полей. `audio_stream::write()` принимает interleaved `float`
samples, а активный поток теперь необходимо остановить через его `voice_handle`.
Старые имена событий также сохранены aliases (`windowResized → window_resized` и т. п.).
Поля событий MNGL используют `window_id`, `device_id`, `content_scale` и другие snake_case
имена. Поля сырых SDL events не изменяются.

Обычные операции аудио и путей теперь возвращают результат напрямую и бросают исключение
при ошибке. Для прежнего `std::expected`-контракта добавьте `try_` к имени операции:
`audio::try_load_sound`, `audio::try_play`, `paths::try_user_data_directory` и т. д.

`graphics_pipeline::raw()` и `graphics_shader::raw()` дают доступ к SDL handle;
прежний `handle()` остаётся совместимым. `dialog::request` поддерживает `ready()` и
`result()`; прежний `poll()` остаётся снимком результата.

`error.hpp` и `runtime.hpp` позволяют подключить базовый слой без логгера, GLM и image API.
Старые глобальные имена доступны через `utilities.hpp`; новый код использует `mngl::`.
Публичные модульные заголовки больше не включают `utilities.hpp` транзитивно. Если старый
код полагался на глобальные aliases или макросы после одного `window.hpp`/`input.hpp`, добавьте
явный `#include <MNGL/utilities.hpp>` либо замените имя на его вариант в `mngl::`.
Заголовок визуализации деревьев находится в `extras/tree_renderer.hpp`; прежний
`tree_renderer.hpp` перенаправляет include для совместимости.

Опечатка `window::flags::resizeable` исправлена на `resizable`; поле нужно переименовать
в пользовательском исходнике. Методы цветов и геометрии получили snake_case имена
(`from_hsv`, `to_hsl`, `intersection_point`, `intersects_line`, `union_with`); старые
вызовы сохраняются wrappers. `current_view()` — основной getter; `get_view()` сохранён.
`mngl::log` и `mngl::logger` заменяют глобальный `logger` и тип `Logger`, которые
пока доступны через `logger.h`/`utilities.hpp` для совместимости. Новый код подключает
`logger.hpp`. `runtime::ensure()` заменяет ручную проверку `SDL_Runtime`.
Глобальные scene-culling и dirty-region настройки переехали в `mngl::gpu`; прежние
`mngl::two_d::enable_*`/`disable_*` функции остаются совместимыми обёртками.
`gpu::device::wait()` теперь бросает при отказе; `try_wait()` возвращает `expected<void, sdl_error>`.

`updatable` имеет стабильную идентичность в update registry и больше не копируется и
не перемещается. Копирование/перемещение производных объектов следует задавать на уровне
их полезных данных, сохраняя отдельную регистрацию каждого живого объекта.
Хеш цвета использует стандартные хеши float без глобальных `COLOR_HASH_*` переключателей;
для собственного алгоритма передайте hasher в контейнер.
`gpu::default_shader_format` заменяет использование `MNGL_DEFAULT_SHADER_FORMAT` в новом коде.

`gpu::device` теперь можно копировать: копии совместно владеют одним SDL GPU device. Обёртки,
созданные с `gpu::device`, удерживают его состояние и могут пережить исходную переменную устройства.
Перегрузки, принимающие сырой `SDL_GPUDevice*`, остаются заимствующими и требуют, чтобы вызывающий
сам обеспечил время жизни устройства. `command_buffer` больше не отправляет команды из деструктора:
добавьте явный `submit()` там, где команды должны выполниться; для отказа от них используйте `cancel()`.
