# Рендер в текстуру

Обычную `mngl::texture` можно использовать как цель рендеринга, а затем нарисовать её содержимое в окне:

```cpp
mngl::render_window window("MNGL 2D", { 800, 600 });
mngl::texture scene(800, 600);

while (mngl::Input::continue_work())
{
    scene.clear();
    scene.draw(
        mngl::rectangle{ 0.1f, 0.1f, 0.3f, 0.2f },
        mngl::color{ 0.9f, 0.3f, 0.2f, 1.0f });
    scene.display();

    window.clear();
    window.draw(scene, mngl::rectangle{ 0.0f, 0.0f, 4.0f / 3.0f, 1.0f });
    window.display();
}
```

Для этого текстура должна иметь usage `color_target`; стандартный конструктор `mngl::texture(width, height)` включает его. Он также включает `sampler`, поэтому результат можно сразу передать в `draw`.

Управлять рендерингом можно и через окно: `window.clear(scene)` начинает кадр с `scene` в качестве цели, затем `window.draw(...)` рисует в неё, а `window.display()` отправляет кадр. Целевая текстура должна жить до `display()`. В этом варианте используется view самой текстуры; view окна применяется только при рендеринге в swapchain.
