# Расширенный 2D-рендеринг

Для обычного рисования используйте [`render_window` и `texture`](index.md). `mngl::two_d::renderer`, `Frame2D` и `mngl::gpu::batcher2d` предназначены для интеграции с собственным GPU lifecycle и более низкоуровневых задач.

## Ручное управление кадром

`two_d::renderer::begin_frame(...)` возвращает `two_d::Frame2D`. В кадр можно отправлять `sprite`, прямоугольники, circle, ellipse и rounded rectangle. `Frame2D::finish(command_buffer)` записывает 2D-команды в переданный command buffer, не отправляя его; после этого вызывающий код сам завершает и отправляет буфер.

У `Frame2D` можно задать пользовательский `view` или полностью собственную view-projection matrix. Эти способы взаимоисключающие: последняя установленная проекция определяет итоговую трансформацию кадра.

## Атласы и материалы

`two_d::texture_store` хранит страницы текстур и создаёт `texture_region` по нормализованным UV-границам. Это удобно для набора спрайтов, собранных в атлас. Для одиночного изображения atlas store не обязателен — передавайте `mngl::texture` напрямую.

`two_d::renderer` также предоставляет создание custom materials и material instances. Материалы используют fragment shader и bindings для uniform blocks, sampled/storage textures и storage buffers. Низкоуровневая настройка этих ресурсов описана в разделе [GPU и шейдеры](../gpu/shaders.md).

`mngl::gpu::batcher2d` — более низкий уровень пакетной отрисовки quad-геометрии. Для пользовательских material instances ресурсы должны принадлежать тому же GPU device и жить не меньше использующих их material instances.
