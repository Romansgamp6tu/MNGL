# Первое окно с отрисовкой

Небольшой проект на C++23 открывает GPU-окно, рисует цветной прямоугольник и
закрывается по Escape. Он не использует собственные картинки, шрифты или
исходники шейдеров.

Сначала установите или распакуйте development-пакет MNGL, затем выполните
команды из корня репозитория:

```powershell
cmake -S examples/first-window -B build/first-window -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build/first-window --config Release
```

Каталог `C:/SDK/MNGL` замените на путь к своему пакету. При запуске также
должны быть доступны runtime-библиотеки, каталог `shaders/` и shader tools.
Подробнее см. [подключение MNGL](../../docs/getting-started/using-mngl.md) и
[быстрый старт 2D](../../docs/2d/quick-start.md).
