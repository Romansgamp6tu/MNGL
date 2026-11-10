# MNGL

MNGL — C++23 библиотека поверх SDL3 для окон, ввода, аудио, 2D-рендеринга
и SDL GPU. Большая часть реализации находится в заголовках; Dear ImGui
собирается в отдельную библиотеку `mngl_imgui`.

## Принципы

Обычные задачи решаются короткими вызовами с разумными настройками по умолчанию.
Детальная настройка и доступ к SDL/GPU подключаются по необходимости; MNGL сохраняет
свободу архитектуры приложения. Правила API, владения, ошибок и оформления закреплены
в [стиле библиотеки](docs/design/api-style.md); изменения имён и контрактов описаны
в [миграции](docs/design/migration.md).

## Возможности

| Модуль | Что доступно |
|---|---|
| Окна и события | Окна, дисплеи, типизированные события SDL и автоматический SDL runtime |
| Идентичность и пути | Метаданные приложения, каталог ресурсов, данные пользователя и стандартные папки ОС |
| Диалоги | Асинхронный выбор файла, нескольких файлов, места сохранения и папки |
| Буфер обмена | Текст и произвольные MIME-форматы с отложенным поставщиком данных |
| Файлы и процессы | Фоновое чтение и запись файлов; запуск команд и долгоживущих помощников |
| input v2 | Отдельные мыши, клавиатуры, joystick, gamepad, touch и pen; состояния устройств, действия и контексты |
| Аудио | Эффекты, музыка, запись, живой PCM-поток, пространственное позиционирование и управляемые голоса |
| 2D | Фигуры, текстуры, текст, view, рендер в текстуру, hit testing и пакетная отрисовка |
| SDL GPU | Устройства, ресурсы, шейдеры, compute, pipeline, проходы и граф кадра |
| Dear ImGui | Интеграция с окнами MNGL и SDL GPU |
| Инструменты | Консольные команды и точки расширения для нативных модов |

API развивается. Эта документация описывает текущий `main`; пакет другого
релиза или ветки может содержать другой набор заголовков.

## Подключение к своему проекту

Для разработки нужен **development-пакет** MNGL: заголовки, библиотеки и
CMake-конфигурация. Сейчас self-hosted runner собирает development- и runtime-
пакеты только для Linux x86_64. CI артефакты доступны в Actions один день, а
при теге `vX.Y.Z` Linux-пакеты прикладываются к
[релизу](https://github.com/Romansgamp6tu/MNGL/releases). Windows x64 и macOS
arm64/x86_64 остаются целевыми платформами, но их автоматическая сборка и
публикация возобновятся после подключения соответствующих native runners.

Распакуйте подходящий пакет и подключите его в `CMakeLists.txt` приложения:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyGame LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE MNGL::MNGL)
```

На Windows с MSVC:

```powershell
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build --config Release
```

`CMAKE_PREFIX_PATH` указывает на корень распакованного пакета. Target
`MNGL::MNGL` передаёт include paths, C++23 и зависимости. DLL из `bin/`
должны быть доступны при запуске. На Linux также нужны системные development
пакеты SDL3, SDL3_image и FreeType.

`MNGL::MNGL` включает SDL3_mixer как транзитивную зависимость. Development- и
runtime-пакеты содержат нужную библиотеку; при использовании CMake target
отдельная настройка SDL3_mixer не нужна. Сборка из исходников клонирует её
вместе с submodule MNGL. Подробности и способы отключения аудиослоя описаны в
[руководстве по аудио](docs/core/audio.md#sdl3-mixer).

Полный пример проекта, подключение из исходников, настройка runtime и
разбор типичных ошибок: [Использование MNGL](docs/getting-started/using-mngl.md).

## Первое окно

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
		while (mngl::input::next_frame())
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

Окно закрывается по Escape или системной кнопке закрытия. Этот пример
показывает окно и ввод; отрисовка описана в [быстром старте 2D](docs/2d/quick-start.md).
Для SDL GPU могут понадобиться дополнительные shader tools и файлы шейдеров.

## Сборка демо из исходников

```powershell
git clone --recurse-submodules https://github.com/Romansgamp6tu/MNGL.git
cd MNGL
cmake -S . -B build -A x64
cmake --build build --config Debug --target mngl_demo
```

Для уже клонированного репозитория выполните `git submodule update --init --recursive`.
В Visual Studio откройте `MNGL.slnx` и выберите `x64`, `Debug` или `Release`.
Windows-сборка использует бинарники из `libs/`; на Linux/macOS зависимости
нужно установить отдельно. Подробности, платформенные ограничения и ручная
сборка пакетов: [Сборка и поставка](docs/getting-started/building.md).
Целевая матрица и обязательные CI-проверки закреплены в
[политике платформ](docs/design/platform-support.md).

## Документация

- [Начало работы и подключение](docs/getting-started/using-mngl.md)
- [Окна, физические устройства, действия и контексты ввода](docs/core/windows-and-input.md)
- [Системные диалоги](docs/core/dialogs.md)
- [Буфер обмена: MIME-форматы и отложенный provider callback](docs/core/clipboard.md)
- [Хранилище и файлы](docs/core/storage.md)
- [Асинхронный ввод-вывод](docs/core/async-io.md)
- [Запуск процессов и помощников](docs/core/processes.md)
- [Runtime, идентичность приложения и пути](docs/core/runtime-and-utilities.md)
- [Аудио](docs/core/audio.md)
- [2D API](docs/2d/index.md) и [SDL GPU](docs/gpu/index.md)
- [Создание модов](docs/mods/creating-and-debugging.md)
- [Карта публичных заголовков](docs/reference/index.md)

Руководство написано в Markdown и собирается через MkDocs:

```powershell
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
python -m mkdocs build --strict
```

Публичные заголовки сторонних библиотек находятся в `include/`, SDL3_mixer —
в `third_party/SDL_mixer`. Полный справочник символов C++ пока не генерируется;
карта модулей ведёт к руководствам и исходным заголовкам.
