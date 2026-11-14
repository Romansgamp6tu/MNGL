# Учебные примеры

В `examples/` лежат два небольших самостоятельных CMake-проекта. Каждый
показывает одну задачу целиком: структуру каталогов, настройку target и
`main.cpp`. Их можно собирать отдельно после установки development-пакета MNGL.

## Первое окно с отрисовкой

Каталог [`examples/first-window`](https://github.com/Romansgamp6tu/MNGL/tree/V-0.2.3-a/examples/first-window)
открывает `render_window`, рисует цветной прямоугольник и закрывает окно по
Escape. Пользовательские изображения, шрифты и файлы шейдеров в проекте не
нужны. При запуске должны быть доступны runtime-библиотеки MNGL, встроенный
каталог `shaders/` и инструменты компиляции шейдеров.

Сборка из корня репозитория в PowerShell:

```powershell
cmake -S examples/first-window -B build/first-window -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build/first-window --config Release
```

На Linux используйте префикс development-пакета, например `/opt/mngl`:

```sh
cmake -S examples/first-window -B build/first-window \
  -DCMAKE_PREFIX_PATH=/opt/mngl -DCMAKE_BUILD_TYPE=Release
cmake --build build/first-window --parallel
```

Путь `C:/SDK/MNGL` замените на каталог распакованного development-пакета.
Инструкции для запуска и Linux есть в
[руководстве по подключению](getting-started/using-mngl.md).

**Что изучить в исходнике:** ресурс создаётся до цикла; в каждом кадре
вызываются `input::next_frame()`, `window.frame()`, `frame.draw()` и
`frame.display()`. Те же шаги подробно разобраны в
[быстром старте 2D](2d/quick-start.md).

## Действия ввода и пауза

Каталог [`examples/input-actions`](https://github.com/Romansgamp6tu/MNGL/tree/V-0.2.3-a/examples/input-actions)
показывает `action_map` с клавишами A/D для движения, Space для прыжка и
Escape для переключения паузы. Для этого примера достаточно обычного окна;
отрисовка и runtime shader assets ему не нужны.

```powershell
cmake -S examples/input-actions -B build/input-actions -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build/input-actions --config Release
```

На Linux команды такие же, замените SDK-префикс:

```sh
cmake -S examples/input-actions -B build/input-actions \
  -DCMAKE_PREFIX_PATH=/opt/mngl -DCMAKE_BUILD_TYPE=Release
cmake --build build/input-actions --parallel
```

В примере два `input_context`: глобальная команда паузы остаётся доступна,
когда выключен игровой контекст. Движение использует удержание действия и
`delta_time()`, а прыжок — одноразовое нажатие. Подробнее о профилях,
переназначении клавиш, нескольких устройствах и потере фокуса читайте в
[рецептах управления](core/input-recipes.md).

## Консольные команды

В репозитории уже есть самостоятельный пример
[`examples/console`](https://github.com/Romansgamp6tu/MNGL/tree/V-0.2.3-a/examples/console).
Он регистрирует `echo`, `sum` и `status`, показывает типизированные аргументы,
флаги, обработку ошибок и работу со state в потоке команд. Он использует
заголовки из исходного checkout, поэтому собирается из корня MNGL:

```sh
cmake -S examples/console -B build-console-example
cmake --build build-console-example --config Release
```

Затем попробуйте `mngl_console_example --help`,
`mngl_console_example --json sum 20 22` или передайте несколько строк через
`--console`. Все варианты для PowerShell и Bash приведены в README примера.
Больше о регистрации команд и контракте обработчиков — в
[руководстве по console API](core/console.md).

## Профилирование 2D

[`examples/2d-benchmark`](https://github.com/Romansgamp6tu/MNGL/tree/V-0.2.3-a/examples/2d-benchmark)
небольшой benchmark для 2D renderer. Это opt-in цель корневого проекта, а не
отдельный потребитель development-пакета. Соберите её из корня репозитория:

```sh
cmake -S . -B build -DMNGL_BUILD_2D_BENCHMARK=ON
cmake --build build --config Release --target mngl_2d_benchmark
```

Клавиши `1`, `2`, `3` выбирают сцены со спрайтами, геометрией и смешанную,
`F2` включает GPU culling, Escape закрывает окно. Benchmark отбрасывает
прогревочные кадры и показывает среднее и 95-й процентиль CPU-таймингов.
Ограничения метрик описаны в [руководстве профилирования](2d/benchmarking.md).

## Моды

Каталог [`examples/mods`](https://github.com/Romansgamp6tu/MNGL/tree/V-0.2.3-a/examples/mods)
содержит host и минимальный нативный мод. Для полного разбора сборки,
загрузки символов и ошибок ABI используйте
[пошаговое руководство по модам](mods/creating-and-debugging.md).

## Если пример не запускается

- Во время сборки CMake должен найти development-пакет: используйте
  `CMAKE_PREFIX_PATH` или точный каталог `MNGLConfig.cmake` через `MNGL_DIR`.
- При запуске ОС ищет DLL/`.so` отдельно от CMake. Разместите runtime рядом
  с программой или добавьте каталог библиотек в путь загрузчика ОС.
- Для `first-window` проверьте каталог `shaders/`, shader tools и GPU backend.
  Успешный запуск `input-actions` проверяет окно и ввод без GPU.
- Запускайте `.exe` из терминала, чтобы сохранить текст ошибки.

См. также [частые ошибки и способы диагностики](getting-started/troubleshooting.md).
