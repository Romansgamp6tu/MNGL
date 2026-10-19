# 2D API

Высокоуровневое 2D API предназначено для обычного рисования без ручной настройки GPU-состояния. Окно и текстура предоставляют похожий цикл кадра:

1. `frame()` начинает кадр и очищает цель.
2. `frame.draw(drawable)` добавляет рисуемый объект.
3. `frame.display()` завершает и отправляет кадр.

Рисуемые объекты реализуют `mngl::drawable`, поэтому для окна, текстуры и `Frame2D` используется одна перегрузка `draw(const drawable&)`. `mngl::rectangle` описывает только геометрию; для заливки используйте drawable `mngl::two_d::rectangle_shape`.

```cpp
auto frame = window.frame();
frame.draw(mngl::two_d::rectangle_shape{ bounds, color });
frame.display();
```

Если frame scope уничтожен без `display()`, он отменяет кадр и освобождает цель. Старый стиль `clear() → draw() → display()` также поддерживается.

Те же операции доступны у `mngl::texture`, поэтому текстура может быть самостоятельной целью рендеринга. Frame scope не владеет окном или целевой текстурой: они должны жить до `display()` либо отмены кадра. Спрайты, фигуры, заливки прямоугольника и текст реализуют `drawable`; изображение показывается через `mngl::sprite`, который хранит его положение, размер и поворот.

## Пользовательские отрисовщики

Для повторно используемого пользовательского объекта унаследуйте класс от `mngl::drawable` и реализуйте `draw(two_d::Frame2D&) const`. После этого объект можно передавать в обычный `draw()` на окне или текстуре — новые перегрузки для него не нужны:

```cpp
class health_bar : public mngl::drawable
{
public:
    explicit health_bar(float value) : value(value) {}

protected:
    void draw(mngl::two_d::Frame2D& frame) const override
    {
        frame.draw(mngl::two_d::rectangle_shape{
            { 0.1f, 0.9f, 0.8f, 0.04f },
            mngl::color{ 0.2f, 0.8f, 0.3f, 1.0f } });
        frame.draw(mngl::two_d::rectangle_shape{
            { 0.1f, 0.9f, 0.8f * value, 0.04f },
            mngl::color{ 0.9f, 0.2f, 0.2f, 1.0f } });
    }

private:
    float value;
};

health_bar health(0.75f);
auto frame = window.frame();
frame.draw(health);
frame.display();
```

Метод `drawable::draw` вызывается синхронно; переданный `Frame2D` нельзя сохранять после вызова. Ресурсы, которые объект передаёт в кадр, должны оставаться живыми до `frame.display()`.

Для одноразовой или захватывающей локальные переменные логики можно передать callback в `draw()`. Он синхронно получает активный `two_d::Frame2D` и может использовать обычные draw-вызовы:

```cpp
auto frame = window.frame();
frame.draw([&](mngl::two_d::Frame2D& frame)
{
    frame.draw(mngl::two_d::rectangle_shape{ background, background_color });
    frame.draw(player_sprite);
});
frame.display();
```

Такой же callback доступен у frame scope render-target `mngl::texture`. Callback выполняется сразу; не сохраняйте переданную ссылку на кадр и не завершайте кадр внутри callback.

## Руководства

- [Быстрый старт](quick-start.md)
- [Рендер в текстуру](render-to-texture.md)
- [Координаты и view](coordinates-and-view.md)
- [Трансформации](transforms.md)
- [Попадания и локальные координаты мыши](hit-testing.md)
- [Рендер текста и большие документы](text-rendering.md)
- [Параллельная запись кадра](parallel-recording.md)
