# Подготовка среды модов и автоматизация

## Локальная подготовка

Скрипты настраивают отдельный каталог сборки и собирают примерный host с модом, чтобы локально запускать и изменять пример. Само графическое приложение `mngl_demo` не собирается. Интеграционная проверка запускается только по явному запросу.

PowerShell (Windows, Visual Studio C++ Build Tools):

```powershell
.\scripts\prepare-mod-env.ps1
.\scripts\prepare-mod-env.ps1 -BuildDirectory build-mods-debug -Configuration Debug
.\scripts\prepare-mod-env.ps1 -Configuration Release -RunTests
```

Bash (Linux, C++23-компилятор и Ninja):

```sh
bash scripts/prepare-mod-env.sh
bash scripts/prepare-mod-env.sh build-mods-debug Debug
bash scripts/prepare-mod-env.sh build-mods Release --test
```

Скрипты не удаляют существующий каталог сборки и не устанавливают системные пакеты. Они используют самостоятельный CMake-проект `examples/mods/CMakeLists.txt`, поэтому на Linux для него нужен только SDL3 development package, CMake 3.24+, Ninja и C++23 toolchain; зависимости графического приложения не требуются. На Windows CMake использует Visual Studio x64 toolchain и SDL3, поставляемый с репозиторием; PowerShell-скрипт ищет CMake в `PATH` или в Visual Studio.

По умолчанию результат находится в `build-mods/`. Host и образец мода доступны в `build-mods/mod-demo/<configuration>/`; запуск host без аргументов загружает образец. Для собственного пакета передайте host-у каталог модов с подпапкой пакета и `mod.json`. После изменений образца пересобирайте `mngl_example_tree_mod`; host пересобирать не нужно. Интеграционный тест создаёт временные пакеты самостоятельно и запускается через `-RunTests` или `--test`. Для генератора с несколькими конфигурациями передавайте одну и ту же конфигурацию скрипту и CTest.

## Контракт для CI/CD

Эту секцию можно использовать при настройке workflow. Локальные скрипты задают воспроизводимую последовательность сборки; workflow вправе вызывать их или повторить те же команды для собственного управления кэшированием и матрицей платформ.

- Включать `BUILD_TESTING=ON` и `MNGL_BUILD_MOD_EXAMPLE=ON` при конфигурации.
- Перед CTest собирать `mngl_mod_integration_test`. Цель зависит от тестовых библиотек модов `mngl_example_tree_mod` и `mngl_test_conflict_mod`.
- Для проверки ручного сценария демо дополнительно собирать `mngl_mod_host` и `mngl_example_tree_mod`.
- На генераторе с несколькими конфигурациями согласованно передавать `--config <Debug|Release>` в build и `-C <Debug|Release>` в CTest. Для Ninja передавать `CMAKE_BUILD_TYPE` при конфигурации.
- Запускать `ctest --test-dir <build-dir> --output-on-failure` после успешной сборки.
- Не зашивать в workflow конкретное имя бинарного файла плагина: CMake передаёт тесту пути через `$<TARGET_FILE:...>`, включая различия `.dll`, `.so` и `.dylib`.
- Тест проверяет ошибочный manifest, несовпадающий ABI, загрузку и отключение функций, применение свойства, конфликт замен, возврат штатного поведения и отложенную выгрузку при живом объекте мода.

Тестовый контракт сосредоточен в `cmake/MNGLMods.cmake` и `tests/mods/integration.cpp`; корневой проект и самостоятельный mod-проект используют одни и те же определения целей. Скрипты являются удобной обёрткой для локальной подготовки и сами не определяют платформенную политику CI.
