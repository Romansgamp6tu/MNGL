# Поверхности и цвета

## Загрузка изображений

Для обычного статичного изображения создайте текстуру сразу по пути:

```cpp
mngl::texture image("assets/background.png");
```

По умолчанию GPU-текстура использует `r8g8b8a8_unorm`. Если для sampling
нужна sRGB-интерпретация или другой порядок каналов, формат можно указать при
загрузке:

```cpp
mngl::texture image("assets/background.png", mngl::gpu::texture_format::r8g8b8a8_unorm_srgb);
```

Surface-загрузка поддерживает RGBA8 и BGRA8 в вариантах UNORM и sRGB. Для иных
GPU-форматов передавайте пиксельные данные в соответствующем представлении.
Формат можно узнать через `image.format()`; `mngl::sprite` использует формат
текстуры, которую он рисует.

Для анимированного изображения, которое нужно рисовать как GPU-текстуру, используйте `mngl::animated_texture`:

```cpp
mngl::animated_texture animation(
    "assets/animation.gif", mngl::gpu::texture_format::r8g8b8a8_unorm_srgb);
auto image = mngl::sprite::borrowed(animation.as_texture(), mngl::rectangle{0.0f, 0.0f, 2.0f, 2.0f});
```

`animated_texture` владеет исходной анимацией и автоматически загружает новый кадр в GPU-текстуру после смены кадра в `input::next_frame()`. `as_texture()` возвращает ссылку: `sprite::borrowed()` не продлевает её жизнь, поэтому объект `animation` должен жить, пока кадр использует спрайт.

Если нужно работать с кадрами напрямую, загрузите файл в `mngl::surface`. Метод `raw()` возвращает указатель на текущий кадр без передачи владения; `animated()` сообщает, содержит ли анимация несколько кадров, а `on_frame_changed` вызывается при переключении кадра. `mngl::texture(surface)` по-прежнему загружает только текущий кадр и не обновляется автоматически.

`surface::update(dt)` принимает конечное неотрицательное время в секундах. Отрицательное или нечисловое значение, переполнение времени либо более 4096 кадров для обработки за один вызов вызывают `std::invalid_argument` до изменения кадра и накопленного времени.

Для разового обновления текстуры можно передать текущую `mngl::surface` или `SDL_Surface*` напрямую: `image.update_from_surface(source)`; пиксели — через `image.update_pixels(std::span<const std::uint8_t>(pixels))`. Surface должен иметь те же размеры и совместимый RGBA8/BGRA8 формат. Эти перегрузки создают и отправляют собственный command buffer на глобальном GPU-устройстве; для пользовательского устройства передайте его явно, а для пакетной загрузки нескольких ресурсов используйте существующий overload с `gpu::command_buffer`.

`mngl::pixel_format` описывает форматы пикселей SDL. Для форматов GPU существует отдельный тип `mngl::gpu::texture_format`; это разные пространства форматов.

## Цвет

`mngl::color` хранит RGBA-компоненты в диапазоне `[0, 1]`. При передаче целых чисел компоненты задаются в диапазоне `[0, 255]`:

```cpp
mngl::color floating{0.2f, 0.6f, 1.0f, 1.0f};
mngl::color bytes{51, 153, 255, 255};
mngl::color html{"#3399FFFF"};
```

Последний компонент альфа-канала можно опускать: по умолчанию он равен полной непрозрачности. Для интеграции с графическими API есть преобразования в `glm::vec3` / `glm::vec4`, `SDL_Color` и целочисленный RGBA-код.

Компоненты доступны напрямую через `r`, `g`, `b`, `a`; векторные представления возвращают `rgb_glsl()`, `rgb_255()`, `rgba_glsl()` и `rgba_255()`. Для целочисленных цветовых литералов используйте стандартные суффиксы с подчёркиванием: `0x3399FF_rgb` или `0x3399FFFF_rgba`.

Дополнительно `color` поддерживает представления HSV/HSL, смешивание, вычисление яркости и преобразования premultiplied alpha. Переводы HSV/HSL, яркость, контраст, `linearize()` и `gamma()` доступны в constant expressions по C++23. HSV/HSL принимают и возвращают нормализованные компоненты `float`; для ввода с градусами и байтовыми насыщенностью/яркостью используйте overload с `glm::uvec3`.

`rgb_uint()` возвращает `0xRRGGBB`, а `rgba_uint()` — `0xRRGGBBAA`.
Строковый конструктор принимает только `#RRGGBB` или `#RRGGBBAA`, без пробелов,
знака и префикса `0x`; регистр HEX-цифр не важен. Любой посторонний символ
приводит к `std::invalid_argument`, даже если перед ним есть корректные цифры.

Для создания оттенка между двумя цветами есть `color::lerp()`. Контраст можно
оценить до выбора цвета текста, а `rgba_uint()` удобно передать в формат или
протокол, который хранит RGBA одним числом. Фрагмент находится внутри функции:

```cpp
const mngl::color background{0.08f, 0.10f, 0.15f};
const mngl::color accent{0.20f, 0.70f, 1.00f};
const mngl::color muted_accent = mngl::color::lerp(accent, background, 0.5f);

const mngl::color white{1.0f};
const float text_contrast = mngl::color::contrast(background, white);
const std::uint32_t accent_rgba = accent.rgba_uint();
```

`lerp(a, b, 0.5f)` даёт средний цвет; коэффициент `0.0f` возвращает `a`, а
`1.0f` — `b`. `contrast()` возвращает отношение яркостей, а не автоматический
выбор подходящей пары. `rgba_uint()` имеет порядок байтов `RRGGBBAA`; это не
порядок SDL `SDL_Color` и не premultiplied-alpha формат.

`color::blends::normal(foreground, background)` выполняет source-over compositing.
Аргументы и результат используют straight alpha: RGB остаётся несвязанным с `a`,
так что результат можно передать в следующий blend без повторного premultiply.
Например, полупрозрачный красный над синим:

```cpp
const mngl::color red{1.0f, 0.0f, 0.0f, 0.5f};
const mngl::color blue{0.0f, 0.0f, 1.0f, 0.5f};
const mngl::color purple = mngl::color::blends::normal(red, blue);
const mngl::color transparent{0.0f, 0.0f, 0.0f, 0.0f};
const mngl::color final_color = mngl::color::blends::normal(purple, transparent);
```

`purple` имеет straight RGB `(2/3, 0, 1/3)` и alpha `0.75`; последующий слой с
полностью прозрачным фоном не затемняет его. Если backend ожидает premultiplied
pixels, создайте копию и вызовите `premultiply()` перед передачей данных:

```cpp
mngl::color gpu_color = purple;
gpu_color.premultiply();
```
