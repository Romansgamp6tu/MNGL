# MNGL

MNGL — C++23 header-oriented библиотека поверх SDL3 для окон, ввода, событий,
2D-рендеринга и SDL GPU. API активно развивается. Текущий Visual Studio/CMake
проект собирает демо-приложение, а не устанавливаемую библиотеку.

## Документация

Руководство находится в `docs/` и публикуется через MkDocs. Инструкция по
сборке, зависимостям и дополнительным shader-инструментам:
[`docs/getting-started/building.md`](docs/getting-started/building.md).

```powershell
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
python -m mkdocs build --strict
```

Для сборки демо через Visual Studio используйте `MNGL.slnx`, платформу `x64`
и конфигурацию `Debug` или `Release`. Экспериментальная сборка через CMake:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

На macOS/Linux CMake ожидает SDL3, SDL3_image и FreeType, установленные в
системе или доступные через `CMAKE_PREFIX_PATH`. Кроссплатформенные сборки и
GPU-бэкенды пока требуют отдельной проверки.

Для автоматической справки по C++ API позднее можно добавить Doxygen и
разместить его результат внутри того же сайта.
