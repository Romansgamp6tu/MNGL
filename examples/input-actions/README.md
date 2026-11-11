# Действия ввода

Полный пример на C++23: A/D задают движение, Space срабатывает один раз при
нажатии, а Escape переключает паузу. Глобальный контекст продолжает работать,
пока игровой контекст выключен.

Собирайте его как отдельный CMake-проект против development-пакета MNGL:

```powershell
cmake -S examples/input-actions -B build/input-actions -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build/input-actions --config Release
```

Примеру нужно обычное окно для цикла событий; отрисовка не требуется. См.
[рецепты управления](../../docs/core/input-recipes.md), где разобраны профили,
переназначение клавиш и время жизни контекстов.
