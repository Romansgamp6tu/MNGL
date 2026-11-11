# Первое 2D-приложение

Этот пример открывает окно и рисует цветной прямоугольник. Для него не нужны картинки и шрифты: сначала проверьте запуск рендера, затем добавляйте ресурсы.

Подготовьте MNGL по [руководству подключения](../getting-started/using-mngl.md). Пример рассчитан на C++23 и target `MNGL::MNGL`. В каталоге приложения достаточно двух файлов:

```text
my_game/
├── CMakeLists.txt
└── main.cpp
```

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyGame LANGUAGES CXX)

find_package(MNGL CONFIG REQUIRED)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE MNGL::MNGL)
```

## Рабочий `main.cpp`

```cpp
#include <MNGL/input.hpp>
#include <MNGL/render_window.hpp>

#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::render_window window("My first MNGL scene", {800, 600});
		mngl::rectangle player{0.2f, 0.2f, 0.25f, 0.25f};

		while (mngl::input::next_frame())
		{
			for (const auto& keyboard : mngl::input::keyboards())
			{
				if (keyboard.pressed(mngl::scancode::escape))
				{
					mngl::input::request_quit();
				}
			}

			auto frame = window.frame(mngl::color{0.07f, 0.09f, 0.14f, 1.0f});
			frame.draw(mngl::two_d::rectangle_shape{
			    player, mngl::color{0.2f, 0.75f, 0.95f, 1.0f}});
			frame.display();
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << "MNGL: " << error.what() << '\n';
		return 1;
	}
}
```

Настройте и соберите приложение с путём к установленному development-пакету:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/SDK/MNGL"
cmake --build build --config Release
```

Запустите полученный executable из подготовленного runtime-каталога. Расположение executable, DLL и шейдеров зависит от способа подключения; [руководство подключения](../getting-started/using-mngl.md) объясняет их размещение. На экране должен появиться голубой квадрат на тёмном фоне. Escape и системная кнопка закрытия завершают приложение.

## Что делает цикл

1. `input::next_frame()` обрабатывает события SDL и обновляет состояния устройств. Возвращённый `false` завершает цикл.
2. `keyboard.pressed()` проверяет переход к нажатому состоянию именно в этом кадре. Список клавиатур может быть пуст, поэтому пример обходит устройства вместо безусловного обращения к первой клавиатуре.
3. `window.frame(clear_color)` начинает кадр и очищает его цветом фона.
4. `frame.draw()` добавляет объект в кадр. `rectangle` хранит геометрию, а `rectangle_shape` является рисуемым объектом.
5. `frame.display()` заканчивает и отправляет кадр на GPU.

`request_quit()` просит завершить цикл при следующем `next_frame()`; текущий кадр ещё может быть нарисован. Если выйти из frame scope без `display()`, кадр отменяется автоматически. Один и тот же объект окна не должен иметь два активных кадра.

## Добавляем движение

Вставьте этот фрагмент внутрь цикла **до** `window.frame()`. Он перемещает прямоугольник клавишами WASD; сам `player` уже объявлен в полном примере:

```cpp
glm::vec2 direction{0.0f, 0.0f};
for (const auto& keyboard : mngl::input::keyboards())
{
	direction.x += keyboard.down(mngl::scancode::d) ? 1.0f : 0.0f;
	direction.x -= keyboard.down(mngl::scancode::a) ? 1.0f : 0.0f;
	direction.y += keyboard.down(mngl::scancode::w) ? 1.0f : 0.0f;
	direction.y -= keyboard.down(mngl::scancode::s) ? 1.0f : 0.0f;
}

const float dt = static_cast<float>(mngl::input::delta_time());
player.pos += direction * (0.4f * dt);
```

Скорость здесь равна `0.4` единицы сцены в секунду по каждой оси. Умножение на `dt` делает перемещение зависимым от времени, а не от количества нарисованных кадров. На диагонали обе оси изменяются одновременно, поэтому итоговая скорость выше; для одинаковой скорости нормализуйте ненулевой `direction` средствами GLM.

`down()` подходит для непрерывного движения. `pressed()` подходит для прыжка, подтверждения меню или переключения паузы. Когда появятся переназначение клавиш и gamepad, замените прямые проверки устройств на [действия и контексты ввода](../core/windows-and-input.md#actions-and-contexts).

## Рисуем изображение

Положите PNG в `assets/image.png` относительно рабочего каталога процесса. Добавьте `#include <MNGL/texture.hpp>`, а после создания окна, **до цикла**, создайте текстуру и спрайт:

```cpp
mngl::texture image("assets/image.png");
mngl::sprite picture{image, mngl::rectangle{0.1f, 0.1f, 0.6f, 0.4f}};
```

В кадре вместо цветного прямоугольника вызовите:

```cpp
frame.draw(picture);
```

Текстура загружается один раз; спрайт хранит ссылку на неё и параметры рисования. Создание текстуры внутри игрового цикла повторяло бы чтение файла и загрузку на GPU каждый кадр. Объект `image` должен жить до отправки каждого использующего его кадра, то есть как минимум до `frame.display()`.

Явный `rectangle` задаёт размер изображения в сцене и может изменить его пропорции. Конструктор `mngl::sprite picture{image};` берёт исходные пропорции из `image.coordinate_size()`. Положение и размер можно менять через `picture.position(...)` и `picture.size(...)`.

Для GIF используется [animated_texture](../core/media-and-colors.md), для кадров из одного PNG — [анимация по атласу](sprite-animation.md).

### Выбор формата текстуры и окна

По умолчанию `mngl::texture("assets/image.png")` загружает изображение в
`r8g8b8a8_unorm`. Чтобы выбрать другой цветовой формат при загрузке обычного
изображения, передайте его явно:

```cpp
mngl::texture image("assets/image.png", mngl::gpu::texture_format::r8g8b8a8_unorm_srgb);
mngl::sprite picture{image, mngl::rectangle{0.1f, 0.1f, 0.6f, 0.4f}};
```

Формат принадлежит `image`, а `sprite` использует эту текстуру как источник.
`texture(path, format)` и загрузка из `surface` поддерживают RGBA8/BGRA8 в
UNORM и sRGB вариантах. Для float, integer и сжатых форматов нужны пиксельные
данные, уже закодированные в совместимом представлении; GPU проверит поддержку
формата и комбинации usage при создании ресурса.

Swapchain окна не принимает произвольный формат: его выбирают SDL и GPU backend.
Можно задать режим композиции HDR/SDR и синхронизацию, а затем узнать фактический
формат текущего swapchain:

Фрагмент ниже находится в `main()` после создания `mngl::render_window window`;
вызывайте настройку между кадрами, когда активного frame scope нет.

```cpp
window.set_swapchain_parameters(
    mngl::gpu::device::hdr_mode::hdr10,
    mngl::gpu::device::vsync_mode::vsync);
const auto window_format = window.swapchain_texture_format();
auto offscreen = window.create_texture({512, 512}, window_format);
```

Передавайте offscreen-формат явно, когда цель должна соответствовать окну.
`create_texture(size)` без формата автоматически использует текущий swapchain.
Настройки меняйте между кадрами; SDL может отклонить HDR-режим, если backend
или дисплей его не поддерживает. Подробнее о текстурах — в [GPU-ресурсах](../gpu/resources.md).

## Что проверить, если результат отличается

| Симптом | Проверка |
|---|---|
| Окно открылось, фигуры не видно | Есть ли `draw()` между `frame()` и `display()`? Находится ли прямоугольник в видимой области? |
| Объект очень маленький или ушёл за экран | Размеры окна в пикселях и размеры сцены различаются. Начните с `rectangle{0.2f, 0.2f, 0.25f, 0.25f}` и прочитайте [координаты](coordinates-and-view.md). |
| PNG не загружается | Путь считается от рабочего каталога процесса, который может отличаться от каталога executable. |
| Исключение о незавершённом кадре | Завершайте каждый кадр через его scope. Не вызывайте `window.frame()` повторно, пока предыдущий кадр активен. |
| GPU или шейдеры не инициализируются | Проверьте backend, runtime-файлы и вывод исключения по [руководству подключения](../getting-started/using-mngl.md). |

Дальше изучите [координаты и камеру](coordinates-and-view.md), [текст](text-rendering.md) и [рендер в текстуру](render-to-texture.md). Когда обычная сцена уже работает, переходите к [геометрии](geometry.md) и [параллельной записи кадра](parallel-recording.md).
