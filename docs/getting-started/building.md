# Сборка и подключение

## Статус проекта

MNGL — header-oriented C++23 библиотека, но текущие Visual Studio и CMake
проекты собирают `mngl_demo`, а не устанавливаемую библиотеку. Готового
`install()`/package target для сторонних приложений пока нет.

Демо проверяет окно, ввод, GPU-рендерер 2D и Dear ImGui. Оно также требует
файлы шейдеров и шрифт, которые сборочные конфигурации копируют в выходную
папку.

## Что находится в репозитории

- `MNGL/` — заголовки MNGL, демо `MNGL.cpp`, HLSL/DXIL шейдеры и исходники
  Dear ImGui.
- `include/SDL3/`, `include/SDL3_image/`, `include/glm/` — заголовки SDL3,
  SDL3_image и GLM.
- `MNGL/imgui/` — Dear ImGui `v1.92.9b-docking`, включая backend-ы
  SDL3/SDL_GPU и файл шрифта `Roboto-Medium.ttf`. Отдельно скачивать ImGui
  для текущей сборки не нужно.
- `libs/` — Windows x64 артефакты SDL3, SDL3_image, SDL3_test и FreeType
  (`.lib` и runtime `.dll`).

`MNGL.exe`, `mngl_demo.exe` и `.pdb` — выходные файлы сборки, не зависимости.
PDB-файлы содержат отладочную информацию и для запуска не нужны.

## Требования

- CMake 3.24 или новее (для CMake-сборки).
- C++23-компилятор и стандартная библиотека с поддержкой используемых
  возможностей, включая `std::format`.
- SDL3, SDL3_image и FreeType, собранные для той же ОС и архитектуры, что и
  приложение.

Для окон, событий, клавиатуры, мыши и геймпадов отдельная input-библиотека
не нужна: MNGL использует SDL3. Демо обрабатывает события через
`mngl::Input::continue_work()`.

### Windows и MSVC

Установите Visual Studio с компонентом **Desktop development with C++** и
CMake. Сборка CMake для x64:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

Если CMake не выбрал желаемую версию Visual Studio, укажите generator явно,
например:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

Также можно открыть `MNGL.slnx` и собрать конфигурацию `Debug|x64` или
`Release|x64`.

В Windows x64 CMake и Visual Studio используют `.lib` из `libs/`; DLL SDL3,
SDL3_image и FreeType копируются в выходную папку приложения. Также туда
копируются шейдеры и шрифт ImGui.

Все `.lib` и DLL должны совпадать с архитектурой программы. Сейчас бинарники в
`libs/` — только x64. Для x86 положите отдельные 32-битные сборки SDL3,
SDL3_image, SDL3_test и FreeType в `libs/x86/` под теми же именами `.lib`,
а соответствующие `SDL3.dll`, `SDL3_image.dll` и `freetype.dll` — туда же.
Win32-конфигурация Visual Studio и x86 CMake-сборка ищут библиотеки именно в
`libs/x86/` и копируют оттуда runtime DLL. Пока эти файлы не добавлены, x86
линковка/сборка не пройдёт. AVX2 не включается для Win32-конфигураций.

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

Установите CMake, C++23-компилятор и development-пакеты SDL3, SDL3_image и
FreeType. CMake должен находить их package-конфигурации; для нестандартного
префикса используйте `CMAKE_PREFIX_PATH`.

```sh
cmake -S . -B build
cmake --build build --parallel
```

Для запуска демо shadercross должен быть доступен во время выполнения: Linux
по умолчанию использует SPIR-V, который собирается из HLSL на старте.
Использование SDL GPU через Vulkan также требует подходящего Vulkan loader и
драйвера GPU.

Кроссплатформенные сборки CMake и реальные GPU-бэкенды пока не проверяются
автоматически; приведённые команды — начальная конфигурация для тестирования.

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
`.dylib` или `.so`. Используйте полную поставку, подходящую ОС/архитектуре, и
держите runtime-компоненты shadercross одной совместимой сборки. CMake пока не
скачивает и не копирует эти optional tools автоматически.

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
| GLM | Векторы и математика; заголовки уже в `include/glm/` | [репозиторий](https://github.com/g-truc/glm), [релизы](https://github.com/g-truc/glm/releases) |
| FreeType | Текущая Windows link-зависимость; `freetype.lib`/`.dll` в `libs/` | [сайт и загрузки](https://freetype.org/download.html), [исходный код](https://gitlab.freedesktop.org/freetype/freetype) |
| Dear ImGui | UI демо; исходники уже в `MNGL/imgui/` | [репозиторий](https://github.com/ocornut/imgui), [релизы](https://github.com/ocornut/imgui/releases) |
| SDL_shadercross | Компиляция и преобразование шейдеров | [репозиторий](https://github.com/libsdl-org/SDL_shadercross), [релизы](https://github.com/libsdl-org/SDL_shadercross/releases) |
| DirectX Shader Compiler (DXC) | Генерация DXIL; Windows runtime `dxcompiler.dll`, `dxil.dll` | [репозиторий](https://github.com/microsoft/DirectXShaderCompiler), [релизы](https://github.com/microsoft/DirectXShaderCompiler/releases) |
| glslang | GLSL → SPIR-V; в Windows Vulkan SDK есть `glslangValidator.exe` | [репозиторий](https://github.com/KhronosGroup/glslang), [релизы](https://github.com/KhronosGroup/glslang/releases), [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) |
| SPIRV-Cross | Преобразование SPIR-V; runtime `spirv-cross-c-shared.dll` на Windows | [репозиторий](https://github.com/KhronosGroup/SPIRV-Cross), [релизы](https://github.com/KhronosGroup/SPIRV-Cross/releases) |
| MoltenVK | Vulkan backend поверх Metal на macOS; не нужен нативному Metal backend | [репозиторий Khronos](https://github.com/KhronosGroup/MoltenVK) |

При распространении бинарников зафиксируйте версии и SHA-256 вместе с лицензиями
и notices upstream-зависимостей. На этом этапе пользователи устанавливают
внешние библиотеки и shader tools самостоятельно.
