# Кэшированные слои

`mngl::two_d::cached_layer` хранит результат отрисовки в render-target текстуре. Содержимое строится callback-ом только когда слой грязный; затем обычный кадр рисует готовую текстуру как один drawable.

```cpp
mngl::two_d::cached_layer background{{1024, 1024}};

if (background.update_if_dirty([&](mngl::two_d::frame_2d& frame)
{
    frame.draw(mngl::two_d::rectangle_shape{{0.0f, 0.0f, 1.0f, 1.0f}, sky_color});
    frame.draw(static_map);
}))
{
    // Cache rebuilt on the first call or after invalidate().
}

auto frame = window.frame();
frame.draw(background);
frame.display();
```

Первый `update_if_dirty()` рисует слой. Пока содержимое не меняется, повторные вызовы сразу возвращают `false` и не создают render pass. Когда меняется карта или другой контент, вызовите `invalidate()`; следующий `update_if_dirty()` перестроит весь слой и вернёт `true`.

Обновляйте кэш до показа слоя в кадре. Попытка нарисовать грязный слой через `frame.draw()` приводит к `std::logic_error`, чтобы случайно не показать устаревшее содержимое. Если callback отрисовки завершился исключением, слой остаётся грязным и его можно построить повторно. Сам `cached_layer` должен жить до `frame.display()`, поскольку кадр использует его текстуру.

Размер конструктора задаётся в пикселях. Как и `texture.frame()`, `cached_layer` использует стандартный глобальный 2D render context. Текстура очищается прозрачным цветом по умолчанию; другой цвет можно указать в конструкторе или задать через `clear_color()`, который также инвалидирует кэш. По умолчанию `destination` использует естественное соотношение сторон текстуры. `destination`, `transform`, `tint` и `blend` управляют тем, как готовый слой рисуется в основном кадре.

MNGL не отслеживает изменения ваших объектов автоматически. `invalidate()` помечает весь слой для перестроения. Для локального изменения вызовите `invalidate_region()` с областью в координатах содержимого слоя, затем используйте callback с областью:

```cpp
layer.invalidate_region(changed_bounds);
layer.update_dirty_regions([&](mngl::two_d::frame_2d& frame, const mngl::rectangle& dirty_bounds)
{
    draw_scene_intersecting(frame, dirty_bounds);
});
```

Если слой ещё не построен или был полностью инвалидирован, `update_dirty_regions()` очищает и строит его целиком, передавая callback полные границы. При локальном обновлении MNGL сохраняет остальную текстуру, очищает каждую dirty-область заданным `clear_color` и ограничивает scissor-ом все draw-команды callback-а. Callback должен заново нарисовать всё содержимое, которое пересекает область, в правильном порядке: так восстановятся соседние объекты после удаления или перемещения. Перекрывающиеся области объединяются. После исключения слой остаётся dirty и требует полного обновления.

Границы слоя задаются в тех же координатах, что и содержимое callback-а; начало координат находится в левом нижнем углу. Регионы округляются до затронутых пикселей, а callback получает фактическую область после округления. Разбивайте сцену на чанки и проверяйте их через `frame.draw_if_visible()`, чтобы callback обходил только объекты, пересекающие dirty-область.

### Пример: перемещение объекта

При перемещении объект перестаёт занимать старую область и начинает занимать новую. Пометьте обе области грязными. В callback восстановите фон и заново нарисуйте **все** элементы, пересекающие грязную область, в исходном порядке слоёв:

В примере `player`, `scene_object`, их `bounds` и обновление позиции принадлежат приложению; это не отдельные типы MNGL. Для объектов сцены храните или вычисляйте их bounds в координатах содержимого слоя:

```cpp
const mngl::rectangle old_bounds = player.bounds;
player.set_position(next_position);
const mngl::rectangle new_bounds = player.bounds;

layer.invalidate_region(old_bounds);
layer.invalidate_region(new_bounds);

layer.update_dirty_regions(
    [&](mngl::two_d::frame_2d& frame, const mngl::rectangle& dirty_bounds)
    {
        frame.draw(background);
        for (const scene_object& object : objects_in_order)
        {
            if (object.bounds.intersects(dirty_bounds))
            {
                frame.draw(object.drawable);
            }
        }
    });
```

Если объект рисует тень или эффект за пределами его основной геометрии, включите этот вынос в `old_bounds` и `new_bounds`. Иначе часть старого изображения останется в кэше или новая часть будет обрезана. Для маленькой сцены проще вызвать `invalidate()` и построить слой целиком; частичный путь полезен, когда callback может быстро найти затронутые чанки и объектов действительно много.

