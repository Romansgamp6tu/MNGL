# Runtime и утилиты

Эта глава объясняет запуск SDL, метаданные приложения, безопасные пути и
общие утилиты. Если вы пока хотите только открыть окно, начните с
[первого приложения](../2d/quick-start.md).

## Инициализация SDL

`mngl::runtime` — единая SDL-инициализация для процесса. MNGL создаёт её через
`mngl::app_runtime`; она настраивает метаданные и вызывает `SDL_Init()` с
`APP_FLAGS`. При обычной конфигурации включены audio, video, gamepad, haptic,
sensor и camera. Макрос `MNGL_CUSTOM_AUDIO_BACKEND` исключает audio из набора
по умолчанию. Если задать `APP_FLAGS`, он полностью заменяет набор по умолчанию.

Ошибка запуска сохраняется в runtime, чтобы приложение могло обработать её
обычным способом. Вызовите `runtime::ensure()` до создания окна, GPU-устройства
или другого ресурса SDL:

```cpp
#include <MNGL/runtime.hpp>

#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::runtime::ensure();
		// Теперь можно создавать ресурсы SDL и MNGL.
	}
	catch (const std::exception& error)
	{
		std::cerr << "Не удалось запустить MNGL: " << error.what() << '\n';
		return 1;
	}
}
```

`runtime.hpp` подключает базовый runtime и тип `mngl::sdl_error`, но не
подключает остальные модули. В старом `utilities.hpp` остаётся глобальный
`SDL_Runtime` для совместимости: его преобразование к `bool` использует
историческое соглашение, где `true` означает ошибку. В новом коде проверяйте
запуск через `mngl::runtime::ensure()` или читайте `running()`, `error()` и
`content()` у `mngl::runtime`.

При завершении процесса runtime вызывает `SDL_Quit()`. Не вызывайте `SDL_Quit()`
вручную, пока приложение использует ресурсы MNGL. Аудиомикшер MNGL управляется
отдельно; подробности — в [руководстве по аудио](audio.md).

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
сообщает ошибки через `std::expected<std::filesystem::path, mngl::sdl_error>`:

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

// Фрагмент из функции приложения.
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
Windows. Для настроек и сохранений используйте user data directory, а не
каталог ресурсов приложения; см. также [хранение данных](storage.md).

Ошибки SDL представлены `mngl::sdl_error`, наследником `std::runtime_error`.
К `what()` добавляется текст SDL, а также место в исходном коде, где создано
исключение. Старые глобальные макросы `TRY` и `TRY_SDL` доступны через
совместимый `utilities.hpp`; новый прикладной код обычно использует методы
MNGL напрямую и обрабатывает исключения в одном понятном месте.

## Логирование

`mngl::log` (тип `mngl::logger`) — готовый общий logger, который пишет в
консоль и `last.log` относительно текущего рабочего каталога. Уровни `log`,
`debug`, `info`, `success`, `warning`, `error` и `critical` доступны из
коробки. Уровень `debug` включён в Debug-сборке и выключен в остальных сборках;
`enable_debug()`, `disable_debug()` и `set_debug(bool)` меняют его во время
работы.

Все уровни добавляют время и метку, например `[INFO]` или `[WARNING]`;
`debug()` ничего не пишет, пока debug-вывод выключен. При необходимости
используйте `mngl::log.set_debug(true)` или `mngl::log.set_debug(false)`.
`flush()` полезен перед завершением, если нужно сразу сбросить буферы файла и
консоли.

Методы logger принимают значения, которые можно вывести через `std::ostream`.
Для форматирования сначала соберите строку, например через `std::format`, а
затем передайте её logger:

```cpp
#include <MNGL/logger.hpp>

#include <format>
#include <string>

int main()
{
	const std::string scene_name{"menu"};
	const int object_count = 18;
	const double load_ms = 3.42;

	mngl::log.info("Сцена загружена: ", scene_name);
	mngl::log.warning("Файл сохранения отсутствует; создаём новый");
	mngl::log.info(std::format("Загружено {} объектов за {:.2f} мс", object_count, load_ms));
	mngl::log.flush();
}
```

## Обновляемые объекты и interop

Наследник `mngl::updatable` автоматически добавляется в `update_manager` при
создании и исключается из обновлений при уничтожении. Каждый успешный вызов
`mngl::input::next_frame()` вызывает `update(double dt)` для зарегистрированных
объектов. `dt` — время с прошлого кадра в секундах. Это удобно для небольшого
глобального таймера или простой анимации; игровые сущности и сложные иерархии
обычно проще обновлять явно из собственного цикла.

Объявление ниже — фрагмент; создайте `elapsed_clock` до обычного игрового цикла,
где уже вызывается `input::next_frame()`.

```cpp
#include <MNGL/iupdate.h>

struct elapsed_clock final : mngl::updatable
{
	double seconds = 0.0;

	void update(double dt) override { seconds += dt; }
};
```

Создайте объект `elapsed_clock` до игрового цикла, и он будет суммировать
время в каждом кадре. В обычном приложении цикл `while
(mngl::input::next_frame())` уже обрабатывает события и вызывает эти обновления.

`mngl::proxy` и alias вроде `mngl::rectangle_proxy` — низкоуровневые адаптеры
для передачи типов MNGL в функции SDL, принимающие указатели на SDL-структуры.
В прикладном коде чаще достаточно неявного преобразования `rectangle` в
`SDL_FRect` или `SDL_Rect`; proxy нужен только API, который требует именно
указатель. `mngl::read_binary_file(path)` читает файл в
`std::vector<std::uint8_t>` и бросает исключение, если файл нельзя открыть,
прочитать или уместить в памяти.

```cpp
#include <MNGL/file_io.hpp>
#include <MNGL/paths.hpp>

// Фрагмент из функции загрузки ресурсов.
const auto asset = mngl::paths::application_directory() / "assets" / "level.bin";
const std::vector<std::uint8_t> bytes = mngl::read_binary_file(asset);
```

Обычные `application_directory()`, `user_data_directory()` и `user_folder_path()`
возвращают `std::filesystem::path` напрямую; при отказе SDL бросается `sdl_error`.
Явный `try_` вариант сохраняет `std::expected` и позволяет обработать отказ как значение.

