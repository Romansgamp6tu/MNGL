# Целевые платформы

MNGL ориентируется на настольные приложения. Мобильные платформы Android и iOS
не входят в текущую цель проекта.

Пакеты macOS рассчитаны на macOS 15 и новее: это целевая версия сборки
зависимостей SDL и Homebrew для обоих поддерживаемых macOS процессоров.

## Матрица поставки

| Платформа | Архитектура | Development-пакет | Runtime-пакет | CI |
|---|---|---|---|---|
| Windows | x64 | ZIP | ZIP | GitHub-hosted runner `windows-2025-vs2026` |
| Linux | x86_64 | tar.gz | tar.gz | GitHub-hosted runner с контейнером Debian Trixie |
| macOS | arm64 | tar.gz | tar.gz | GitHub-hosted runner `macos-15` |
| macOS | x86_64 | tar.gz | tar.gz | GitHub-hosted runner `macos-15-intel` |

Workflows `Package MNGL development kits` и `Package MNGL runtime` собирают
временные Actions-артефакты на push и при ручном запуске. Артефакты хранятся
один день. При push тега `vX.Y.Z` все development- и runtime-пакеты
прикладываются к GitHub Release. Для каждой платформы используются обычные
GitHub-hosted runners; Linux-пакеты собираются в Debian Trixie контейнере.

Development-пакет Linux проверяется через отдельный CMake-проект-потребитель.
Runtime-пакеты запускают аудио smoke-проверку и проверку доступности GPU или
графического backend-а на соответствующей платформе; GPU-проверка может быть
пропущена, если адаптер или backend недоступен. Полная CTest-матрица остаётся
отдельным Linux workflow на self-hosted runner-е; полноценные CTest-запуски
Windows/macOS пока не входят в CI.

## Обязательства по платформе

- Публичные заголовки и базовый CMake-проект должны компилироваться на всех
  перечисленных платформах.
- CI собирает development- и runtime-пакеты для Windows x64, Linux x86_64,
  macOS arm64 и macOS x86_64.
- Runtime-пакет должен включать динамические зависимости MNGL и инструменты,
  необходимые встроенным шейдерам; зависимости ОС, графический драйвер и
  системные фреймворки остаются на целевой машине.
- macOS использует Metal. HLSL сначала компилируется glslang в SPIR-V, затем
  shadercross/SPIRV-Cross преобразует SPIR-V в MSL.
- Linux CI проверяет сборку и упаковку, но не обещает совместимость каждой
  версии драйвера или физического GPU.
