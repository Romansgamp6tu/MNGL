# Использование MNGL в своём проекте

MNGL можно подключить из development-пакета через `find_package()` или
из исходников через `add_subdirectory()`. В обоих случаях приложение
линкуется с CMake target `MNGL::MNGL` и включает нужные заголовки.

Для работы нужен C++23-компилятор со стандартной библиотекой, поддерживающей
`std::format` и `std::expected`. В Windows development-пакет рассчитан на
MSVC x64. Сборка самого MNGL и пакетов описана [отдельно](building.md).

## Выбор пакета

| Поставка | Содержимое | Когда нужна |
|---|---|---|
| Development | Заголовки, CMake config/targets, библиотека ImGui; в Windows также import libraries и runtime DLL | Компиляция своего приложения |
| Runtime | Динамические библиотеки, shader tools, встроенные шейдеры и манифест | Запуск и распространение приложения |
| Исходники | Код MNGL, демо, моды, тесты и заголовки зависимостей | Разработка библиотеки и подключение свежего API |

Development и runtime пакеты собираются для Windows x64 и Linux x64.
macOS пока требует подключения из исходников и самостоятельной установки
зависимостей. Скачивайте пакеты одной версии и подходящей архитектуры:
заголовки, import libraries и runtime должны соответствовать друг другу.

Workflow `Package MNGL development kits` в репозитории создаёт development
артефакты, `Package MNGL runtime` — runtime артефакты. В тегированных релизах
они прикладываются к GitHub Release. Сборка архива не подтверждает работу
каждого GPU backend на пользовательском оборудовании.

Документация описывает текущий `main`. В частности, input v2 и `audio.hpp`
могут отсутствовать в более раннем пакете. `MNGL::MNGL` автоматически подключает
SDL3_mixer в пакетах, которые содержат audio v1 или новее; его возможности и
варианты отключения описаны в [руководстве по аудио](../core/audio.md#sdl3-mixer).

## Минимальный CMake-проект

Создайте каталог приложения с двумя файлами:

```text
my-game/
  CMakeLists.txt
  main.cpp
```

В `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyGame LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE MNGL::MNGL)
```

Target передаёт требование C++23, include paths, линковку ImGui и базовых
зависимостей. Публичные файлы MNGL из пакета `main` находятся в
`include/MNGL/`; используйте `#include <MNGL/window.hpp>` и аналогичные пути.
Короткие подключения вроде `#include "window.hpp"` тоже поддерживаются через
include paths target. Единого обязательного umbrella-заголовка нет.

В `main.cpp`:

```cpp
#include <MNGL/input.hpp>
#include <MNGL/window.hpp>
#include <exception>
#include <iostream>
#include <string>

int main()
{
	try
	{
		mngl::runtime::ensure();
		mngl::window window("My game", {800, 600});
		while (mngl::input::continue_work())
		{
			for (const auto& keyboard : mngl::input::keyboards())
			{
				if (keyboard.pressed(mngl::scancode::escape))
				{
					mngl::input::should_work = false;
				}
			}
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
```

SDL запускается автоматически. `mngl::runtime::ensure()` проверяет запуск и
бросает `mngl::sdl_error` при ошибке. После успешного запуска `input::continue_work()`
обрабатывает события и обновляет ввод один раз за итерацию. Проверять
`pressed()` и `released()` следует после этого вызова. Не добавляйте второй
цикл `SDL_PollEvent()`, который заберёт события у MNGL.

Настройте `APP_NAME`, `APP_VERSION`, стабильный `APP_ID`,
`APP_ORGANIZATION` и `APP_STORAGE_NAME` до подключения заголовков MNGL.
Они задают метаданные SDL и расположение пользовательских данных. Пример
конфигурации и API каталогов находится в [руководстве runtime](../core/runtime-and-utilities.md#app-identity).

Этот пример создаёт обычное окно без GPU-отрисовки и не требует файлов
шейдеров. Закрыть его можно Escape или системной кнопкой закрытия.

## Конфигурация и запуск

На Windows с MSVC укажите корень распакованного development-пакета:

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build --config Release
$env:PATH = "C:/SDK/MNGL/bin;" + $env:PATH
./build/Release/my_game.exe
```

Вместо изменения `PATH` можно разместить DLL из `bin/` рядом с `.exe`.
Поиск CMake-пакета и поиск DLL при запуске настраиваются отдельно:
`CMAKE_PREFIX_PATH` помогает найти заголовки и библиотеки при сборке.

На Linux установите development-пакеты SDL3, SDL3_image и FreeType, затем:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/mngl -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
LD_LIBRARY_PATH="/opt/mngl/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ./build/my_game
```

Здесь `/opt/mngl` — корень распакованного development-пакета. Если нужны
несколько SDK, `CMAKE_PREFIX_PATH` принимает список префиксов. Можно также
указать `MNGL_DIR` непосредственно на каталог с `MNGLConfig.cmake`, обычно
`<prefix>/lib/cmake/MNGL`.

`MNGL::ImGui` экспортируется отдельно. Для обычного приложения используйте
`MNGL::MNGL`, который уже связывает его с базовыми зависимостями.

## Подключение из исходников

Клонируйте репозиторий вместе с SDL3_mixer submodule:

```sh
git clone --recurse-submodules https://github.com/Romansgamp6tu/MNGL.git
```

Если папки `my-game/` и `MNGL/` лежат рядом, используйте такой `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyGame LANGUAGES CXX)

set(BUILD_TESTING OFF CACHE BOOL "Build MNGL tests")
set(MNGL_BUILD_MOD_EXAMPLE OFF CACHE BOOL "Build MNGL mod examples")
add_subdirectory("../MNGL" "${CMAKE_BINARY_DIR}/mngl")

add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE MNGL::MNGL)
```

Конфигурация MNGL также объявляет target демо. Чтобы собрать только своё
приложение и его зависимости, укажите `--target my_game` в команде сборки.
Если `BUILD_TESTING` уже задан родительским проектом, пример выше не меняет
его кешированное значение. Локальная сборка MNGL копирует DLL и assets для
своего демо; runtime собственного приложения нужно разместить отдельно.

Для уже клонированного MNGL выполните `git submodule update --init --recursive`.
В Windows исходники используют `.lib` из `libs/`, в Linux/macOS CMake ищет
системные SDL3, SDL3_image и FreeType. CMake options аудио нужно задать до
`add_subdirectory()`; подробности — в [руководстве по аудио](../core/audio.md).

При создании собственного install tree включите
`MNGL_BUILD_WINDOWS_DEVEL_PACKAGE` или `MNGL_BUILD_LINUX_DEVEL_PACKAGE`:
install rules и экспорт CMake config объявляются именно в этих режимах.
Команды приведены в [руководстве по сборке](building.md).

## Рендеринг и данные приложения

Для рисования используйте `render_window.hpp` и [быстрый старт 2D](../2d/quick-start.md).
Development-пакет содержит заголовки и библиотеки; встроенные файлы
`shaders/` и shader tools поставляются runtime-пакетом либо берутся из
исходников и устанавливаются самостоятельно. Расположите runtime `shaders/`
рядом с исполняемым файлом. Нужные программы компиляции шейдеров должны
быть доступны через `PATH` или явно заданы в настройках shader loader.
Подробности: [Шейдеры](../gpu/shaders.md).

Пользовательские текстуры, звуки и шрифты — данные приложения. CMake MNGL
не копирует их в сторонний проект. Пути вроде `assets/image.png` задаются
относительно текущего рабочего каталога; задайте его в IDE или используйте
явные пути. Для текста также нужен файл шрифта; пример его загрузки есть в
[руководстве по тексту](../2d/text-rendering.md).

## Частые проблемы

| Симптом | Что проверить |
|---|---|
| `MNGLConfig.cmake` не найден | Нужен development-пакет; укажите его корень в `CMAKE_PREFIX_PATH` или каталог config в `MNGL_DIR` |
| Не найден `MNGL/*.hpp` | Приложение должно линковаться с `MNGL::MNGL`; пакет должен содержать используемый API |
| Не найдена DLL или `.so` | Разместите runtime рядом с приложением либо настройте `PATH`/`LD_LIBRARY_PATH` |
| Не определены символы `MIX_*` | Проверьте, что приложение подключено к `MNGL::MNGL` и использует пакет с SDL3_mixer; при ручной настройке проверьте import library и defines |
| Не найден SDL3 CMake package на Linux | Установите development-пакеты зависимостей или добавьте их префикс в `CMAKE_PREFIX_PATH` |
| Окно есть, а 2D не запускается | Проверьте GPU backend, драйвер, `shaders/` и shader tools; простому `mngl::window` они не нужны |
| Не найден звук, шрифт или изображение | Проверьте рабочий каталог и путь к данным приложения |
| Предупреждения о `input::is_key_*` | Перейдите на устройства или `action_map` по [руководству по вводу](../core/windows-and-input.md) |

## Дальнейшее использование

- [Окна и input v2](../core/windows-and-input.md): физические устройства, состояния, `action_map` и контексты.
- [Аудио](../core/audio.md): эффекты, потоковая музыка, захват, живые PCM-источники и позиционирование.
- [Системные диалоги](../core/dialogs.md): выбор файлов, папок и места сохранения.
- [Буфер обмена](../core/clipboard.md): текст и произвольные данные с MIME-типами.
- [Хранилище и файлы](../core/storage.md): ресурсы, пользовательские данные и перечисление каталогов.
- [2D](../2d/index.md) и [GPU](../gpu/index.md): два уровня доступа к отрисовке.
- [Runtime](../core/runtime-and-utilities.md): metadata, флаги запуска и логирование.
- [Карта модулей](../reference/index.md): выбор публичного заголовка для задачи.
