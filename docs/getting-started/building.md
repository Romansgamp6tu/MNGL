# Сборка и подключение

Для готового CMake-проекта приложения начните с [Использования MNGL](using-mngl.md).
Эта страница описывает сборку самого репозитория, зависимости и создание пакетов.

## Статус проекта

MNGL — header-oriented C++23 библиотека. Обычная сборка CMake собирает
`mngl_demo`; на Windows demo использует отдельную DLL `mngl_imgui`, а на Linux
ImGui собирается в `libmngl_imgui.so`. Предусмотрены development-пакеты с
заголовками и CMake targets: Windows x64 ZIP и Linux x64 tar.gz. Отдельные
runtime-пакеты собираются для Windows x64 и Linux x86_64.

В `main` реализованы input v2 и аудио поверх SDL3_mixer. Корневой CMake-проект
собирает микшер из submodule, связывает его с `MNGL::MNGL` и добавляет библиотеку
в development- и runtime-пакеты. Для сборки из исходников submodule нужен
рекурсивный checkout; отдельные шаги линковки приложения не требуются.
Подробности: [руководство по аудио](../core/audio.md#sdl3-mixer).

CMake также собирает отдельную консольную цель `mngl_mod_host` и динамический
мод `mngl_example_tree_mod`. Запуск host показывает загрузку мода и вызов
расширенной функции, свойства и фабрики класса. Подробности находятся в
[руководстве по созданию и отладке модов](../mods/creating-and-debugging.md).

Демо проверяет окно, ввод, GPU-рендерер 2D и Dear ImGui. Оно также требует
файлы шейдеров и шрифт, которые сборочные конфигурации копируют в выходную
папку.

## Что находится в репозитории

- `MNGL/` — заголовки MNGL, демо `MNGL.cpp`, HLSL/DXIL шейдеры и исходники
  Dear ImGui.
- `include/SDL3/`, `include/SDL3_image/`, `include/glm/` — заголовки SDL3,
  SDL3_image и GLM.
- `third_party/SDL_mixer/` — Git submodule SDL3_mixer `release-3.2.4`; корневой
  CMake собирает динамическую библиотеку микшера со встроенными декодерами
  распространённых форматов.
- `MNGL/imgui/` — Dear ImGui `v1.92.9b-docking`, включая backend-ы
  SDL3/SDL_GPU и файл шрифта `Roboto-Medium.ttf`. Отдельно скачивать ImGui
  для текущей сборки не нужно.
- `libs/` — Windows x64 библиотеки SDL3, SDL3_image, SDL3_test и FreeType
  (`.lib`), а также runtime DLL для SDL3, SDL3_image и FreeType.

`MNGL.exe`, `mngl_demo.exe` и `.pdb` — выходные файлы сборки, не зависимости.
PDB-файлы содержат отладочную информацию и для запуска не нужны.

## Требования

- CMake 3.24 или новее (для CMake-сборки).
- C и C++23-компиляторы и стандартная библиотека с поддержкой используемых
  возможностей, включая `std::format` и `std::expected`.
- SDL3, SDL3_image и FreeType, собранные для той же ОС и архитектуры, что и
  приложение.

Для окон, событий, клавиатуры, мыши и геймпадов отдельная input-библиотека
не нужна: MNGL использует SDL3. Демо обрабатывает события через
`mngl::input::continue_work()`.

### Получение исходников

```sh
git clone --recurse-submodules https://github.com/Romansgamp6tu/MNGL.git
cd MNGL
```

Для существующей копии выполните `git submodule update --init --recursive`.
SDL3_mixer и его встроенные декодеры собираются CMake автоматически.

### Windows и MSVC

Установите Visual Studio с компонентом **Desktop development with C++** и
CMake. Сборка CMake для x64:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug --target mngl_demo
```

Если CMake не выбрал желаемую версию Visual Studio, укажите generator явно,
например:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target mngl_demo
```

Также можно открыть `MNGL.slnx` и собрать конфигурацию `Debug|x64` или
`Release|x64`.

В Windows x64 CMake использует `.lib` из `libs/` и собирает SDL3_mixer из
исходников. В выходную папку приложения копируются `mngl_imgui.dll`, DLL SDL3,
SDL3_image и FreeType, шейдеры и шрифт ImGui. Runtime DLL микшера также входит
в собранные выходные файлы и пакеты.

#### Development-пакеты

GitHub Actions собирает Windows x64 ZIP на GitHub-hosted `windows-latest` runner
и Linux x64 tar.gz на self-hosted runner при push и для ручного запуска workflow.
При push тега `vX.Y.Z` оба пакета прикладываются к GitHub Release. Для ручной
сборки Windows-пакета:

```powershell
cmake -S . -B build-package -A x64 `
  -DMNGL_BUILD_WINDOWS_DEVEL_PACKAGE=ON `
  '-DMNGL_PACKAGE_VERSION=1.2.3'
cmake --build build-package --config Release --target mngl_imgui --parallel 4
cmake --build build-package --config Release --target SDL3_mixer-shared --parallel 4
cpack --config build-package/CPackConfig.cmake -C Release -G ZIP
```

Артефакт появится в `build-package/packages/`. Он содержит заголовки,
`mngl_imgui.dll`, SDL3 и SDL3_mixer с import libraries, CMake configs, лицензии
и targets `MNGL::MNGL` и `MNGL::ImGui`. GitHub Actions после упаковки
распаковывает архив и собирает из него CMake-потребителя с аудиосмоук-проверкой.
Подключение в приложении:

```cmake
find_package(MNGL CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE MNGL::MNGL)
```

Заголовки проекта автоматически попадают из `MNGL/**/*.h` и `MNGL/**/*.hpp`
в `include/MNGL`; содержимое vendored `MNGL/imgui/` в этот набор не входит.
Новые заголовки feature-ветки, которых ещё нет в `main`, сохраняют свой путь
в `include/<имя-ветки>/` (например, `tree/foo.hpp` станет
`include/tree-practicum/tree/foo.hpp`). Для сравнения с `main` workflow получает
полную историю Git. Extra include включают SDL3, SDL3_image, GLM и публичные
заголовки используемых ImGui API и SDL3/SDL GPU backend; остальные внутренние
заголовки ImGui не копируются. При ручной сборке feature-ветки укажите её через
`-DMNGL_PACKAGE_BRANCH=<имя-ветки>` и убедитесь, что `origin/main` доступен.

Linux x64 development-пакет собирается на self-hosted runner с теми же
development-пакетами SDL3, SDL3_image и FreeType, что нужны runtime-сборке:

```sh
cmake -S . -B build-devel -GNinja -DCMAKE_BUILD_TYPE=Release \
  -DMNGL_BUILD_LINUX_DEVEL_PACKAGE=ON \
  -DMNGL_PACKAGE_VERSION=1.2.3
cmake --build build-devel --target mngl_imgui --parallel 4
cmake --build build-devel --target SDL3_mixer-shared --parallel 4
cpack --config build-devel/CPackConfig.cmake -G TGZ
```

В `build-devel/packages/mngl-devel-linux-x64-1.2.3.tar.gz` находятся
`libmngl_imgui.so`, `libSDL3_mixer.so`, заголовки, CMake configs, лицензии и
targets `MNGL::MNGL` и `MNGL::ImGui`.
При подключении `find_package(MNGL CONFIG REQUIRED)` на Linux автоматически
ищутся системные CMake-пакеты SDL3, SDL3_image и FreeType; они должны быть
установлены в окружении разработчика.

Папку ZIP `bin/` нужно добавить в `PATH` или скопировать её DLL рядом с
исполняемым файлом. Версии ImGui между DLL и приложением должны совпадать:
ImGui не гарантирует совместимость ABI между версиями.

#### Runtime-пакеты

Workflow `Package MNGL runtime` собирает Windows x64 на GitHub-hosted runner,
а Linux x86_64 — на self-hosted runner с метками `self-hosted`, `linux`, `x64`.
Для Linux runner нужны CMake, Ninja, Git, `pkg-config`, Bison, `patchelf`,
`readelf`, SDL3/SDL3_image/FreeType development-пакеты и XRandR development
пакет. На Ubuntu их можно установить один раз:

```sh
sudo apt update
sudo apt install build-essential cmake ninja-build git pkg-config bison patchelf binutils \
  libxrandr-dev flex libjson-perl libvulkan1 libsdl3-dev libsdl3-image-dev libfreetype-dev
```

На push в `main` или ручной запуск workflow runtime ZIP/tar.gz доступен как
Actions artifact. При push тега `vX.Y.Z` оба runtime-пакета также добавляются к
GitHub Release:

- `mngl-runtime-windows-x64-X.Y.Z.zip`
- `mngl-runtime-linux-x64-X.Y.Z.tar.gz`

В архивах находятся `mngl_imgui.dll`/`libmngl_imgui.so`, SDL3, SDL3_image,
SDL3_mixer, FreeType и их переносимые зависимости, лицензии микшера и
встроенных декодеров,
`shadercross`, `glslangValidator`,
встроенные `shaders/` и манифест. Demo, headers, `.lib` и статические
development-библиотеки в runtime-пакет не входят. Распакуйте архив в корень
папки приложения: тогда `shaders/` окажется рядом с исполняемым файлом,
откуда MNGL загружает встроенные шейдеры.

На Windows добавьте `bin/` к `PATH`; в архив также входят MSVC runtime DLL для
x64. На Linux добавьте `bin/` в `PATH`, а `lib/` в `LD_LIBRARY_PATH`:

```sh
export PATH="/path/to/mngl-runtime/bin:$PATH"
export LD_LIBRARY_PATH="/path/to/mngl-runtime/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

Linux-пакет собирается на Ubuntu 26.04 x86_64. glibc, Vulkan loader
(`libvulkan1`), GPU-драйверы и системные графические/аудио-библиотеки не
копируются в архив; для запуска нужна совместимая Ubuntu-система с установленным
драйвером.

Все `.lib` и DLL должны совпадать с архитектурой программы. Сейчас бинарники в
`libs/` — только x64. Для x86 положите отдельные 32-битные сборки SDL3,
SDL3_image, SDL3_test и FreeType в `libs/x86/` под теми же именами `.lib`,
а соответствующие `SDL3.dll`, `SDL3_image.dll` и `freetype.dll` — туда же.
Win32-конфигурация Visual Studio и x86 CMake-сборка ищут библиотеки именно в
`libs/x86/` и копируют оттуда runtime DLL. Пока эти файлы не добавлены, x86
линковка/сборка не пройдёт. AVX2 не включается для Win32-конфигураций.

### Параметры CMake

| Параметр | По умолчанию | Назначение |
|---|---|---|
| `BUILD_TESTING` | `ON` | Объявляет API, audio и GPU runtime smoke-проверки |
| `MNGL_BUILD_MOD_EXAMPLE` | `ON` | Собирает host, пример мода и связанные mod targets |
| `MNGL_CUSTOM_AUDIO_BACKEND` | `OFF` | Отключает аудиоинтеграцию MNGL целиком |
| `MNGL_CUSTOM_AUDIO_INTERFACE` | `OFF` | Отключает фасад и встроенную интеграцию SDL3_mixer |
| `MNGL_CUSTOM_MIXER_INTERFACE` | `OFF` | Отключает только фасад `mngl::audio` |
| `MNGL_BUILD_WINDOWS_DEVEL_PACKAGE` | `OFF` | Добавляет Windows x64 install rules и CPack ZIP |
| `MNGL_BUILD_LINUX_DEVEL_PACKAGE` | `OFF` | Добавляет Linux x64 install rules и CPack TGZ |
| `MNGL_PACKAGE_VERSION` | `0.0.0` при включённой упаковке | Версия создаваемого development-пакета |

Например, для демо без mod targets и тестов:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=OFF -DMNGL_BUILD_MOD_EXAMPLE=OFF
cmake --build build --config Debug --target mngl_demo
```

Свойства аудио применяются ко всем потребителям source target `mngl`.
Режимы и их приоритет описаны в [настройках аудио](../core/audio.md#audio-configuration).
В готовом CMake-пакете это уже экспортированные настройки: изменения
одноимённых options стороннего проекта не пересобирают MNGL.

### macOS

Нужны Xcode Command Line Tools, CMake и SDL3/SDL3_image/FreeType. Например,
с Homebrew:

```sh
brew install cmake sdl3 sdl3_image freetype
cmake -S . -B build-macos -DCMAKE_PREFIX_PATH="$(brew --prefix)"
cmake --build build-macos --parallel
```

На macOS SDL GPU использует нативный Metal backend, когда SDL выбирает его;
MoltenVK для этого не нужен. Для текущего демо встроенные шейдеры HLSL
компилируются во время запуска в MSL, поэтому `shadercross` и его runtime
зависимости должны быть доступны приложению. См. [Шейдеры](../gpu/shaders.md).

### Linux

Установите CMake, C-компилятор, C++23-компилятор и development-пакеты SDL3, SDL3_image и
FreeType. CMake должен находить их package-конфигурации; для нестандартного
префикса используйте `CMAKE_PREFIX_PATH`.

```sh
cmake -S . -B build
cmake --build build --parallel
```

Для запуска демо shadercross должен быть доступен во время выполнения: Linux
по умолчанию использует SPIR-V, который собирается из HLSL на старте. Его и
`glslangValidator` можно взять из runtime-пакета выше. Использование SDL GPU
через Vulkan также требует подходящего Vulkan loader и драйвера GPU.

Runtime workflow запускает аудиосмоук-тест с SDL dummy audio и проверяет
создание SDL GPU device на runner-е. GPU-проверка помечается как пропущенная,
если на runner-е недоступны video/GPU backend или адаптер; это не заменяет
тесты рендеринга на физических GPU и драйверах.

## Shader tools и дополнительные бинарники

Компилятор шейдеров не нужен для компиляции MNGL-заголовков. Он нужен во время
выполнения для рендерера на системах, где нет подходящего заранее собранного
байткода, а также при вызове `mngl::gpu::shader_loader`.

- `shadercross` — HLSL → MSL/SPIR-V/DXIL и другие поддерживаемые SDL форматы.
  Для HLSL MNGL вызывает его напрямую.
- `glslangValidator` — GLSL → SPIR-V. Нужен для исходных GLSL-шейдеров.
  Чтобы преобразовать GLSL из SPIR-V в другой формат, нужны оба инструмента.
- Для Windows HLSL→DXIL нужны совместимые runtime-компоненты DXC
  (`dxcompiler.dll`, `dxil.dll`) и shadercross.
- Динамический shadercross может требовать SDL shadercross runtime и
  SPIRV-Cross runtime: например, `SDL3_shadercross.dll` и
  `spirv-cross-c-shared.dll` на Windows.

По умолчанию MNGL ищет программы `shadercross` и `glslangValidator` через
`PATH`. Для других имён/расположений задайте
`shader_compile_options::shadercross_executable` и
`shader_compile_options::glslang_validator_executable`. В Windows-бинарном
наборе может находиться `glslang.exe`; укажите это имя явно или переименуйте
файл в `glslangValidator.exe`.

Поставки инструментов различаются: у них могут быть дополнительные DLL,
`.dylib` или `.so`. Runtime workflow собирает инструменты для Windows x64 и
Linux x86_64, а обычная локальная CMake-сборка их не скачивает и не копирует.
macOS runtime-пакет пока не собирается.

DXIL-файлы в `MNGL/shaders/` рассчитаны на Windows; на macOS и Linux
используются соответствующие MSL/SPIR-V пути и HLSL-файлы. MoltenVK — Vulkan
поверх Metal, а не shader compiler; он нужен только если приложение явно
выбирает Vulkan backend на macOS. Для нативного Metal пути он не требуется.

## Upstream и официальные загрузки

Ссылки ниже ведут на проекты и официальные страницы загрузки. Точные версии,
параметры сборки и контрольные суммы DLL/`.lib`, уже лежащих в `libs/`, в
репозитории не зафиксированы; поэтому нельзя подтвердить, из какого именно
релиза получен каждый текущий бинарник.

| Зависимость | Для чего нужна | Upstream / загрузки |
|---|---|---|
| SDL3 | Окна, события, ввод, SDL GPU | [репозиторий](https://github.com/libsdl-org/SDL), [релизы](https://github.com/libsdl-org/SDL/releases) |
| SDL3_image | Загрузка изображений | [репозиторий](https://github.com/libsdl-org/SDL_image), [релизы](https://github.com/libsdl-org/SDL_image/releases) |
| SDL3_mixer | Эффекты, потоковая музыка, пространственное микширование и живые источники; собирается из рекурсивного submodule вместе с декодерами | [репозиторий](https://github.com/libsdl-org/SDL_mixer), [релизы](https://github.com/libsdl-org/SDL_mixer/releases) |
| GLM | Векторы и математика; заголовки уже в `include/glm/` | [репозиторий](https://github.com/g-truc/glm), [релизы](https://github.com/g-truc/glm/releases) |
| FreeType | Текущая Windows link-зависимость; `freetype.lib`/`.dll` в `libs/` | [сайт и загрузки](https://freetype.org/download.html), [исходный код](https://gitlab.freedesktop.org/freetype/freetype) |
| Dear ImGui | UI демо; исходники уже в `MNGL/imgui/` | [репозиторий](https://github.com/ocornut/imgui), [релизы](https://github.com/ocornut/imgui/releases) |
| SDL_shadercross | Компиляция и преобразование шейдеров | [репозиторий](https://github.com/libsdl-org/SDL_shadercross), [релизы](https://github.com/libsdl-org/SDL_shadercross/releases) |
| DirectX Shader Compiler (DXC) | Генерация DXIL; Windows runtime `dxcompiler.dll`, `dxil.dll` | [репозиторий](https://github.com/microsoft/DirectXShaderCompiler), [релизы](https://github.com/microsoft/DirectXShaderCompiler/releases) |
| glslang | GLSL → SPIR-V; в Windows Vulkan SDK есть `glslangValidator.exe` | [репозиторий](https://github.com/KhronosGroup/glslang), [релизы](https://github.com/KhronosGroup/glslang/releases), [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) |
| SPIRV-Cross | Преобразование SPIR-V; runtime `spirv-cross-c-shared.dll` на Windows | [репозиторий](https://github.com/KhronosGroup/SPIRV-Cross), [релизы](https://github.com/KhronosGroup/SPIRV-Cross/releases) |
| MoltenVK | Vulkan backend поверх Metal на macOS; не нужен нативному Metal backend | [репозиторий Khronos](https://github.com/KhronosGroup/MoltenVK) |

При распространении бинарников зафиксируйте версии и SHA-256 вместе с лицензиями
и notices upstream-зависимостей. Для ручной сборки устанавливайте нужные
библиотеки и shader tools самостоятельно либо используйте соответствующий
runtime-пакет. SDL3_mixer входит в development- и runtime-пакеты MNGL.
