# Сборка и подключение

## Сборка репозитория

Сейчас репозиторий содержит Visual Studio solution `MNGL.slnx` и проект-пример `MNGL/MNGL.vcxproj`. Проект настроен на C++23 и toolset MSVC `v145`; для сборки выберите платформу `x64` и конфигурацию `Debug` или `Release`.

Из Developer PowerShell с установленным MSBuild:

```powershell
MSBuild.exe MNGL.slnx /m /p:Platform=x64 /p:Configuration=Debug /v:minimal
```

Заголовки SDL3, SDL3_image и GLM лежат в `include/`, а используемые проектом `.lib` и runtime `.dll` — в `libs/`. Проект копирует нужные runtime DLL в выходную папку приложения.

Dear ImGui `v1.92.9b-docking` хранится в `MNGL/imgui/`; проект напрямую компилирует исходники ImGui и backend-ы SDL3/SDL_GPU. Обёртка MNGL в `MNGL/imgui_compat.hpp` header-only и отдельного `.cpp` не требует. Отдельно скачивать или устанавливать ImGui для текущей сборки не требуется.

## Подключение MNGL в собственный проект

Отдельного CMake target или готового package/export target в репозитории пока нет: текущий Visual Studio project собирает исполняемый пример, а не устанавливаемую библиотеку. Поэтому это руководство не описывает автоматическое подключение MNGL к стороннему приложению. Для такого сценария нужно отдельно настроить include paths, linking библиотек SDL и копирование runtime DLL; способ распространения MNGL стоит зафиксировать до появления готовой инструкции.

Для шейдеров, компилируемых во время выполнения, дополнительно нужны инструменты из раздела [Шейдеры](../gpu/shaders.md).
