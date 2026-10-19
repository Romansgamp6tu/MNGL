# Рендер в текстуру

Обычную `mngl::texture` можно использовать как цель рендеринга, а затем нарисовать её содержимое в окне:

```cpp
mngl::render_window window("MNGL 2D", { 800, 600 });
mngl::texture scene(800, 600);
mngl::sprite scene_sprite{ scene, mngl::rectangle{ 0.0f, 0.0f, 4.0f / 3.0f, 1.0f } };

while (mngl::Input::continue_work())
{
    auto scene_frame = scene.frame();
    scene_frame.draw(mngl::two_d::rectangle_shape{
        { 0.1f, 0.1f, 0.3f, 0.2f },
        mngl::color{ 0.9f, 0.3f, 0.2f, 1.0f } });
    scene_frame.display();

    auto window_frame = window.frame();
    window_frame.draw(scene_sprite);
    window_frame.display();
}
```

Для этого текстура должна иметь usage `color_target`; стандартный конструктор `mngl::texture(width, height)` включает его. Он также включает `sampler`, поэтому результат можно использовать как источник `mngl::sprite`.

Если scope уничтожен без `display()`, рендеринг кадра отменяется; это также происходит при выходе из scope из-за исключения. `window.frame(scene)` начинает кадр окна с `scene` в качестве цели. Целевая текстура должна оставаться живой, пока кадр активен. В этом варианте используется view самой текстуры; view окна применяется только при рендеринге в swapchain.

Для независимых сцен, которые нужно параллельно отрисовать в разные offscreen-текстуры и затем собрать в окне, см. [руководство по параллельному рендеру](parallel-recording.md).
