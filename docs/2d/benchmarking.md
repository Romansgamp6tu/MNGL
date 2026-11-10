# Профилирование 2D-кадра

`mngl_2d_benchmark` — интерактивная сцена для сравнения CPU-стоимости обычной отрисовки и пользовательской геометрии.
Профилировщик можно включить отдельно в собственном приложении:

```cpp
auto& renderer = window.renderer();
renderer.enable_profiling();

// Сразу после frame.display():
const auto& stats = renderer.last_frame_stats();
const auto flush_ms = std::chrono::duration<double, std::milli>(stats.flush_cpu_time).count();
```

Время в `frame_stats` хранится в `std::chrono::nanoseconds`; поля равны нулю для кадров, записанных при выключенном профилировании.
Параметр профилирования меняют между кадрами.

| Поле | Что измеряет |
|---|---|
| `frame_recording_wall_time` | Wall time от `begin_frame()` до начала `flush()`, включая работу вызывающего кода и ожидание параллельных контекстов |
| `culling_cpu_time` | CPU-подготовку метаданных для GPU-culling; проверку видимости выполняет GPU |
| `upload_cpu_time` | Создание staging-загрузок и запись copy-команд |
| `command_encoding_cpu_time` | Запись compute- и render-команд на стороне CPU |
| `flush_cpu_time` | CPU-время внутри `batcher2d::flush()`, без command-buffer submission |

Время записи команд не равно времени их исполнения GPU. Публичный SDL GPU API не предоставляет timestamp-запросов для GPU-проходов, поэтому бенчмарк не может показать чистое время compute- или render-прохода. Режим `--wait-gpu` после каждого кадра вызывает [`SDL_WaitForGPUIdle()`](https://wiki.libsdl.org/SDL3/SDL_WaitForGPUIdle) и выводит `GPU idle wait`: это время ожидания завершения отправленной работы на стороне CPU, включая очередь и драйвер, а не аппаратный GPU timestamp. Такой режим синхронизирует каждый кадр и предназначен для сравнения завершения небольшого фиксированного workload.

Обычный запуск также выводит CPU-время `frame()`, `frame.draw()` и `frame.display()`. В оконном режиме `display` может включать получение swapchain-изображения и present. `--no-present` рисует ту же сцену в offscreen color target, не получает swapchain texture и не предъявляет кадр окну. Окно остаётся открытым для событий и выхода по `Escape`.

## Сборка и запуск сцены

Включите цель при конфигурации корневого проекта:

```powershell
cmake -S . -B build -A x64 -DMNGL_BUILD_2D_BENCHMARK=ON
cmake --build build --config Release --target mngl_2d_benchmark
./build/Release/mngl_2d_benchmark.exe
```

Для Ninja или Unix Makefiles используйте `-DCMAKE_BUILD_TYPE=Release` при конфигурации и запустите `build/mngl_2d_benchmark`.
Сцена получает шейдеры из выходного каталога сборки.

Клавиши переключают нагрузку: `1` рисует спрайты, `2` — triangle-list геометрию, `3` — обе группы, `F2` включает или выключает GPU-culling, `Escape` закрывает окно. GPU-culling проверяет спрайты и resident meshes по инстансам, а динамическую геометрию — по блокам до 64 треугольников. Инстансы не компактизируются: все исходные данные загружаются, а видимые draw calls сохраняют исходный порядок. Для невидимых спрайтов и resident meshes GPU пропускает transform в vertex shader, но число instance vertex invocations не сокращается. Невидимые блоки динамической геометрии пропускаются через indirect draw. Сравнивайте режимы на каждом типе нагрузки: GPU-путь может передавать больше данных, когда большая часть объектов видима.

Параметры запуска:

```text
mngl_2d_benchmark --sprites 20000 --triangles 20000 --offscreen 70
mngl_2d_benchmark --mode geometry --gpu-culling on --sprites 0 --triangles 20000 --offscreen 70
mngl_2d_benchmark --mode geometry --gpu-culling off --no-present --wait-gpu --sprites 0 --triangles 20000 --offscreen 70
mngl_2d_benchmark --mode geometry --gpu-culling on --wait-gpu --sprites 0 --triangles 20000 --offscreen 70
```

`--offscreen` задаёт долю элементов за границей области отрисовки от 0 до 100. `--sprites` и `--triangles` задают размеры нагрузок; каждый параметр можно отключить нулём. `--mode` выбирает `sprites`, `geometry` или `mixed`, а `--gpu-culling` — начальное состояние GPU-culling (`on`/`off`); эти аргументы позволяют запускать сравниваемые режимы отдельно без клавиатурного ввода. `--no-present` отключает вывод в окно, а `--wait-gpu` включает ожидание завершения GPU после кадра и автоматически выбирает offscreen target. `--help` выводит подсказку.

После запуска и каждого переключения режима сцена пропускает 120 кадров на прогрев, затем выводит среднее и 95-й перцентиль CPU-времени за последние 120 кадров. В оконном режиме benchmark отключает стандартный лимит MNGL в 60 FPS и запрашивает у SDL режим `IMMEDIATE`, чтобы не мерить ожидание лимита или VSync. Если backend не поддерживает `IMMEDIATE`, запуск завершится с понятной ошибкой. Этот режим может вызывать разрывы изображения. Строки `input`, `frame begin`, `draw calls` и `display`/`finish+submit` замеряются снаружи библиотеки; `input` также включает ожидание событий и обновление состояния ввода. Остальные значения читаются из `last_frame_stats()`. В конце показываются число batch-ей, draw calls, инстансов и отправленных байтов.

Для сравнения используйте одинаковые разрешение, количество элементов, GPU backend и состояние VSync. Проценты загрузки CPU/GPU сами по себе не показывают узкий участок; сопоставляйте измеренные интервалы и отдельно учитывайте время ожидания в `frame()` или `display()`.
