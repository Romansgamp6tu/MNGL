# Runtime и утилиты

## Инициализация SDL

MNGL автоматически инициализирует SDL через singleton, доступный как
глобальный `SDL_Runtime`, и завершает его при завершении программы. По
умолчанию включены подсистемы SDL для audio, video, gamepad, haptic, sensor
и camera. `MNGL_CUSTOM_AUDIO_BACKEND` исключает audio из стандартных флагов.
Собственный `APP_FLAGS` полностью задаёт состав подсистем.

Проверьте запуск до создания ресурсов; ошибка попадёт в обычный обработчик исключений:

```cpp
#include <MNGL/runtime.hpp>
mngl::runtime::ensure();
```

`runtime.hpp` подключает только базовый runtime и ошибки. Старый глобальный
`SDL_Runtime` остаётся в `utilities.hpp` для совместимости: его `true` означает ошибку.

Внутренний аудиомикшер создаётся отдельно при первом аудиовызове и
уничтожается раньше SDL runtime. Не вызывайте `SDL_Quit()` вручную, пока
ресурсы MNGL используются приложением. Подробности — в [руководстве по аудио](audio.md).

<a id="app-identity"></a>

## Идентичность приложения

Задайте настройки до первого подключения заголовков MNGL. Они должны
совпадать во всех единицах трансляции: используйте общий конфигурационный
заголовок или compiler definitions.

```cpp
#define APP_NAME "My game"
#define APP_VERSION 1.2.0
#define APP_ID "com.example.mygame"
#define APP_ORGANIZATION "Example Studio"
#define APP_STORAGE_NAME "My game"
#define APP_CREATOR "Example Studio"  // необязательно
#define APP_URL "https://example.com" // необязательно
#define APP_TYPE "game"               // необязательно
#include <MNGL/paths.hpp>
```

`APP_NAME` — отображаемое имя, `APP_VERSION` — версия, а `APP_ID` — уникальный
идентификатор в обратном доменном формате. ID должен оставаться постоянным
между версиями приложения. `APP_ORGANIZATION` и `APP_STORAGE_NAME` задают
организацию и стабильное имя каталога пользовательских данных. Если имя для
хранения не задано, оно по умолчанию совпадает с `APP_NAME`; задайте его явно,
если отображаемое имя может измениться. Для этих двух имён используйте буквы,
цифры и пробелы. Для приложений, которые сохраняют пользовательские данные,
замените все значения-заглушки.

`APP_VERSION` преобразуется в строку самим MNGL, поэтому в примере он записан
без кавычек. `APP_CREATOR`, `APP_COPYRIGHT`, `APP_URL` и `APP_TYPE` —
необязательные строковые литералы. Для `APP_TYPE` SDL принимает `"game"`,
`"mediaplayer"` или `"application"`. MNGL передаёт эти метаданные SDL до
`SDL_Init()`. `APP_FLAGS` задаётся битовой комбинацией флагов SDL.

## Пути приложения и пользователя

`<MNGL/paths.hpp>` предоставляет пути SDL как `std::filesystem::path` и
сообщает ошибки через `std::expected<path, sdl_error>`:

- `mngl::paths::try_application_directory()` — каталог ресурсов приложения.
  На macOS это может быть `Resources` внутри `.app` bundle. Используйте его
  для чтения упакованных данных, не для сохранений.
- `mngl::paths::try_user_data_directory()` — персональный каталог приложения для
  настроек и сохранений. SDL создаёт его при первом запросе, используя
  `APP_ORGANIZATION` и `APP_STORAGE_NAME`.
- `mngl::paths::try_user_folder_path(folder)` — стандартный каталог пользователя:
  `home`, `desktop`, `documents`, `downloads`, `music`, `pictures`,
  `public_share`, `saved_games`, `screenshots`, `templates` или `videos`.
  Некоторые каталоги доступны не на каждой платформе.

```cpp
#include <MNGL/paths.hpp>
#include <filesystem>
#include <iostream>

auto data = mngl::paths::try_user_data_directory();
if (!data)
{
	std::cerr << data.error().what() << '\n';
}
else
{
	const auto saves = *data / "saves";
	std::filesystem::create_directories(saves);
}

auto downloads = mngl::paths::try_user_folder_path(mngl::paths::user_folder::downloads);
```

Рабочий каталог процесса зависит от того, откуда приложение запустили; он не
заменяет каталог ресурсов или пользовательских данных. SDL возвращает пути в
UTF-8, а MNGL преобразует их в `std::filesystem::path`, в том числе для
Windows.

Для ошибок SDL используется глобальный `sdl_error`, наследующийся от `std::runtime_error` и добавляющий текст последней ошибки SDL. Внутренние проверки библиотеки используют макросы `TRY` и `TRY_SDL`.

## Логирование

`mngl::log` (тип `mngl::logger`) пишет сообщения в консоль и `last.log`. Доступны уровни `log`, `debug`, `info`, `success`, `warning`, `error` и `critical`; `debug` по умолчанию включён в Debug-сборке. `enable_debug()`, `disable_debug()` и `set_debug(bool)` меняют его во время работы, а `flush()` сбрасывает буферы вывода.

```cpp
logger.info("Loaded texture: ", image.width(), " x ", image.height());
logger.error("Failed to load scene");
```

## Обновляемые объекты и interop

Наследник `mngl::updatable` регистрируется при создании и удаляется из `update_manager` при уничтожении. `input::continue_work()` вызывает его `update(double dt)` каждый кадр. Этот механизм рассчитан на простые глобальные обновления, например анимацию, а не на сложные иерархии объектов.

`mngl::proxy` и alias вроде `rectangle_proxy` — низкоуровневые адаптеры для передачи типов MNGL в функции SDL, принимающие указатели на SDL-структуры. Для обычного прикладного кода обращаться к ним напрямую не требуется. `read_binary_file(path)` — утилита чтения файла в `std::vector<uint8_t>`.

Обычные `application_directory()`, `user_data_directory()` и `user_folder_path()`
возвращают `std::filesystem::path` напрямую; при отказе SDL бросается `sdl_error`.
Явный `try_` вариант сохраняет `std::expected` и позволяет обработать отказ как значение.
