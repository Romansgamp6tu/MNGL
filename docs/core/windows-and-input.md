# Окна и ввод

## Окно и дисплеи

`mngl::window` — RAII-обёртка окна SDL. Конструктор принимает заголовок, размер и необязательные флаги:

```cpp
mngl::window::flags flags{};
flags.resizable = true;

mngl::window window("My app", {1280, 720}, flags);
```

Флаги окна задаются полями `window::flags`; среди них fullscreen, borderless, high-pixel-density, modal и platform graphics backend. Большинство свойств окна читаются и меняются через пары методов вроде `size()` / `size(new_size)` и `title()` / `title(new_title)`. Для 2D-отрисовки используйте `mngl::render_window`, который расширяет окно рендерингом.

`window::size()` возвращает размер окна в оконных координатах, а `pixel_size()` — фактический размер drawable-области в пикселях. Они могут различаться на дисплеях с высокой плотностью пикселей.

`mngl::display` позволяет получить список дисплеев (`displays()`), основной дисплей (`primary()`), границы, рабочую область, масштаб и видеорежимы. Окно предоставляет удобные методы `attached_display()` и `mode()`.

## Основной цикл и состояние ввода

Обычно приложение вызывает `input::continue_work()` один раз за кадр:

```cpp
#include "input.hpp"

while (mngl::input::continue_work())
{
	for (const auto& keyboard : mngl::input::keyboards())
	{
		if (keyboard.pressed(mngl::scancode::a))
		{
			// Обработать нажатие в текущем кадре.
		}
	}

	const double dt = mngl::input::get_dt();
	// Обновить состояние приложения и отрисовать кадр.
}
```

`continue_work()` обрабатывает события, обновляет состояние ввода и вызывает `update(dt)` у зарегистрированных `mngl::updatable`. Он возвращает `false` после запроса закрытия приложения или когда `input::should_work` сброшен. `get_dt()` возвращает время между кадрами в секундах.

Старые `input::is_key_*`, `input::is_mouse_button_*` и `input::get_mouse_*` сохранены на время миграции и помечены `[[deprecated]]`. Запросы клавиш и кнопок мыши в этом фасаде объединяют состояния всех физических устройств; позиция берётся у первой мыши, а дельты движения и прокрутки суммируются. Новые методы позволяют обращаться к каждому устройству отдельно. Например, старый вызов `input::is_key_pressed(scancode::a)` заменяется на `input::keyboard().pressed(scancode::a)` после проверки, что список клавиатур не пуст. Для кнопок и положения мыши используйте `input::mice()` и методы полученных объектов:

```cpp
const auto keyboards = mngl::input::keyboards();
if (!keyboards.empty() && keyboards.front().pressed(mngl::scancode::a))
{
	// Нажатие на первой клавиатуре в текущем кадре.
}

for (const auto& mouse : mngl::input::mice())
{
	if (mouse.pressed(mngl::mouse_button::left))
	{
		const glm::vec2 position = mouse.position();
		// Нажатие и координаты принадлежат этой мыши.
	}
}
```

Устройства также предоставляют `down()` и `released()`. Положение, смещение и прокрутка каждой мыши доступны через `position()`, `delta()` и `scroll_delta()`.

## Отдельные клавиатуры и мыши

`input::keyboards()` и `input::mice()` возвращают снимок подключённых устройств в порядке SDL. Для быстрого доступа к устройству по номеру из текущего списка есть `input::keyboard(num_from_list = 0)` и `input::mouse(num_from_list = 0)`:

```cpp
const auto keyboards = mngl::input::keyboards();
const auto mice = mngl::input::mice();

if (!keyboards.empty())
{
	const auto keyboard = mngl::input::keyboard(); // первое устройство
	if (keyboard.pressed(mngl::scancode::space))
	{
		// Нажатие пробела на этой клавиатуре в текущем кадре.
	}
}

if (!mice.empty())
{
	const auto mouse = mngl::input::mouse(); // первая мышь
	if (mouse.down(mngl::mouse_button::left))
	{
		const glm::vec2 position = mouse.position();
		// position относится к окну mouse.window_id().
	}
}
```

Список — снимок на момент вызова. Для сохранения ссылки на устройство храните полученный объект `keyboard` или `mouse`: он содержит ID устройства, поэтому смена порядка списка не меняет выбранное устройство. `name()` возвращает имя, `connected()` — состояние подключения. Если устройств нет или номер вне диапазона, одиночный метод бросает `std::out_of_range`; сначала проверьте список, когда отсутствие устройства допустимо.

Состояние клавиш накапливается по keyboard ID из SDL-событий. `SDL_GetKeyboardState()` возвращает общее состояние и не позволяет отдельно опрашивать несколько клавиатур.

У устройств доступны `pressed()`, `down()` и `released()`. Первое и последнее состояние действуют один кадр, `down()` отражает состояние после обработки событий этого кадра. Потеря фокуса, отключение устройства или уничтожение окна сбрасывает удерживаемые кнопки и даёт однократный `released()`. `release_reason()` возвращает `input_release_reason::physical_release`, `focus_lost`, `device_removed`, `window_destroyed`, `mapping_changed` или `canceled`, если причина применима к устройству. Методы устройств показывают физическое состояние независимо от захвата ImGui; старые статические запросы `input::is_key_*` и `input::is_mouse_button_*` сохраняют прежнюю фильтрацию.

У мыши `has_position()` показывает, получена ли позиция из события. `position()` — последняя позиция в логических координатах окна, `window_id()` — ID этого окна. `delta()` и `scroll_delta()` накапливаются за кадр для этого окна; при смене окна предыдущие дельты сбрасываются. Список устройств и имена запрашивайте на главном потоке, в том же потоке, где вызывается `input::continue_work()`.

Состояние мышей собирается из событий SDL отдельно по ID устройства. SDL-функции `SDL_GetMouseState()` и `SDL_GetGlobalMouseState()` не принимают ID мыши, поэтому не позволяют опрашивать кнопки и координаты нескольких устройств независимо.

## Сенсорные устройства

`input::touch_devices()` возвращает список SDL touch-устройств, а `input::touch_device(num_from_list = 0)` выбирает устройство по индексу и бросает `std::out_of_range`, если его нет. Список синхронизируется при каждом `input::continue_work()` и при запросе. На некоторых платформах SDL обнаруживает touch-устройство только после первого касания. Обёртка `touch_device` хранит ID устройства; `name()`, `type()` и `connected()` дают его свойства. Искусственные touch-события, создаваемые SDL из мыши или пера, в список не включаются.

```cpp
const auto devices = mngl::input::touch_devices();
if (!devices.empty())
{
	const auto touch = mngl::input::touch_device();
	for (const auto& finger : touch.fingers())
	{
		if (finger.pressed())
		{
			// Новое касание в этом кадре.
		}

		const glm::vec2 normalized = finger.position();
		const uint32_t window_id = finger.window_id();
	}
}
```

`fingers()` включает активные касания и касания, завершившиеся в текущем кадре; порядок отсортирован по ID пальца. `finger(id)` возвращает `std::optional<touch_finger>`. У пальца доступны `pressed()`, `down()`, `released()`, `release_reason()`, нормализованные `position()` и `delta()`, `pressure()` в диапазоне SDL `[0, 1]` и `window_id()`. `pressed()` и `released()` действуют один кадр; при быстром касании и отпускании в одном кадре оба могут быть `true`. `position_in_window(logical_size)` переводит нормализованные координаты в логические координаты окна умножением на его размер; для устройств типа `SDL_TOUCH_DEVICE_DIRECT` это координаты окна, а у indirect-устройств сохраняется система координат SDL.

Обычное завершение даёт причину `physical_release`, системная отмена touch-события — `canceled`. Потеря фокуса или уничтожение окна отпускает активные касания с соответствующей причиной. Если SDL больше не перечисляет устройство, активные касания отпускаются с причиной `device_removed`.

## Перья

SDL сообщает ID пера через события, а отдельного API перечисления и событий подключения/отключения у этой версии нет. `input::pens()` возвращает перья, уже замеченные SDL в текущем запуске, в порядке возрастания ID; `input::pen(num_from_list = 0)` выбирает объект по индексу. Список может быть пуст до первого события пера; выход из зоны близости не удаляет перо из списка. `type()` возвращает `SDL_PenDeviceType`, который SDL может определить как `UNKNOWN`.

```cpp
const auto pens = mngl::input::pens();
if (!pens.empty())
{
	const auto stylus = mngl::input::pen();
	if (stylus.in_proximity())
	{
		const glm::vec2 position = stylus.position();
		const float pressure = stylus.pressure();
		const glm::vec2 tilt = stylus.tilt();
	}

	if (stylus.down())
	{
		// Кончик касается поверхности.
	}
}
```

У `pen` доступны `id()`, `type()`, `in_proximity()`, `has_position()`, `position()`, `delta()`, `window_id()` и `is_eraser()`. Координаты пера SDL задаёт в логических координатах окна; дельта суммирует перемещение за кадр. `pressed()`, `down()` и `released()` относятся к контакту кончика. Кнопки корпуса доступны отдельно через `button_pressed()`, `button_down()`, `button_released()` и `button_release_reason()` с `pen_button::button1`–`button5`.

`axis(pen_axis_type)` возвращает значение SDL без дополнительной нормализации; также есть удобные `pressure()` и `tilt()`. SDL сообщает pressure, наклоны, расстояние до поверхности, вращение, slider и tangential pressure. Оси, которые платформа или перо не поддерживает, остаются равны нулю. Выход пера из зоны близости отпускает удерживаемый кончик и кнопки с причиной `canceled`; потеря фокуса или уничтожение окна использует соответствующую причину сброса.

## Джойстики

`input::joysticks()` возвращает снимок устройств SDL в текущем порядке, а `input::joystick(num_from_list = 0)` выбирает устройство по индексу. Если список пуст или индекс вне диапазона, одиночный метод бросает `std::out_of_range`. Полученный объект привязан к ID устройства, поэтому дальнейшая смена порядка списка его не перенаправляет.

```cpp
const auto joysticks = mngl::input::joysticks();
if (!joysticks.empty())
{
	const auto stick = mngl::input::joystick(); // первый джойстик

	if (stick.pressed(0))
	{
		// Кнопка 0 нажата в этом кадре.
	}

	const float x = stick.normalized_axis(0); // диапазон [-1, 1]
	const mngl::hat_position dpad = stick.hat(0);
}
```

У `joystick` доступны `id()`, `name()`, `type()`, `connected()` и количества `button_count()`, `axis_count()`, `hat_count()`, `ball_count()`. Индексы кнопок, осей, hats и шаров начинаются с нуля. Кнопки имеют состояния `pressed()`, `down()`, `released()` и `release_reason()` с той же кадровой семантикой, что у клавиатуры. `axis()` возвращает исходное значение SDL в диапазоне `[-32768, 32767]`, `normalized_axis()` — значение в `[-1, 1]`. `hat()` возвращает `hat_position`; `ball_delta()` — накопленное движение trackball за кадр. Для несуществующего индекса кнопка возвращает `false`, ось — `0`, hat — `hat_position::none`, ball — нулевую дельту.

При отключении джойстика удерживаемые кнопки отпускаются с причиной `device_removed`, оси обнуляются, hats переходят в `centered`. Состояние самого джойстика глобальное и не сбрасывается при потере фокуса окна. Список следует обновлять, чтобы получить новые устройства после hot-plug. SDL включает в список джойстиков и устройства, распознанные как gamepad; стандартная раскладка доступна через `input::gamepads()`.

При открытии джойстика кнопки, оси и hats считываются с его SDL handle. Затем изменения приходят из событий конкретного устройства и входят в снимок кадра `input::continue_work()`.

Ограничение частоты кадров по умолчанию равно 60 FPS. Вызов `set_max_fps(0)` отключает ожидание между кадрами.

## Игровые контроллеры

`input::gamepads()` возвращает устройства, для которых SDL нашла стандартную раскладку. `input::gamepad(num_from_list = 0)` выбирает контроллер по индексу из текущего списка и бросает `std::out_of_range`, если элемента нет. Объект `gamepad` привязан к ID устройства и не зависит от дальнейшего порядка списка.

```cpp
const auto gamepads = mngl::input::gamepads();
if (!gamepads.empty())
{
	const auto pad = mngl::input::gamepad();
	if (pad.pressed(mngl::gamepad_button::a))
	{
		// Стандартная кнопка A / south нажата в текущем кадре.
	}

	const float x = pad.normalized_axis(mngl::gamepad_axis::leftx);
	if (pad.has_rumble())
		pad.rumble(0x4000, 0x4000, 250);
}
```

Кнопки используют раскладку SDL: `a`, `b`, `x`, `y` соответствуют позициям south, east, west и north независимо от надписей на корпусе. Также доступны back, guide, start, стики, плечевые кнопки, крестовина, touchpad, paddles и дополнительные кнопки. Состояния `pressed()`, `down()`, `released()` и `release_reason()` имеют ту же семантику, что у клавиатуры. После смены раскладки отпускание получает причину `mapping_changed`.

Оси возвращаются в исходном диапазоне SDL методом `axis()` и в виде `[-1, 1]` для стиков или `[0, 1]` для триггеров через `normalized_axis()`. Dead zone не применяется. `has_button()` и `has_axis()` позволяют проверить, присутствует ли элемент в раскладке устройства. `button_label()` возвращает стандартную подпись кнопки, известную SDL.

Для сведений о контроллере доступны `name()`, `path()`, `type()`, `real_type()`, `guid()`, `connection_state()`, `player_index()`, vendor/product/version, firmware, serial, Steam handle и `battery_info()`. `set_player_index()` назначает игрока или снимает назначение значением `-1`.

SDL-специфичные возможности тоже доступны через `gamepad`: `mapping()` и `bindings()` читают текущую раскладку, `set_mapping()` меняет её; `touchpad_count()`, `touchpad_finger_count()` и `touchpad_finger()` читают touchpad; методы sensor включают датчики и читают данные с заданным числом значений. `rumble()`, `rumble_triggers()`, `set_led()` и `send_effect()` выполняют аппаратные эффекты. Перед вызовом эффекта проверяйте соответствующие `has_rumble()`, `has_trigger_rumble()`, `has_rgb_led()`, `has_player_led()` или `has_mono_led()`: поддержка зависит от модели.

Глобальные методы `input::has_gamepad()` и `input::is_gamepad(id)` проверяют наличие и SDL-совместимость устройства. Для пользовательских раскладок `input::add_gamepad_mapping(text)` и `input::add_gamepad_mappings_from_file(path)` возвращают SDL-код: `1` — добавлена, `0` — заменена существующая, `-1` — ошибка; `input::reload_gamepad_mappings()` перечитывает базу SDL. `gamepad_mappings()`, `gamepad_mapping_for_id()` и `gamepad_mapping_for_guid()` читают сохранённые в SDL раскладки. Сырые данные устройства по-прежнему доступны через `input::joysticks()`.

Контроллеры доступны глобально, не привязаны к окну и не сбрасываются при потере фокуса. При отключении удерживаемые кнопки отпускаются с причиной `device_removed`, а оси обнуляются. Запросы к объектам `gamepad` выполняйте на том же потоке, что и `input::continue_work()`.

При открытии gamepad кнопки и оси считываются с его SDL handle; дальше состояние обновляется по событиям конкретного gamepad ID внутри `input::continue_work()`.

<a id="actions-and-contexts"></a>

## Действия и контексты ввода

`mngl::action_map` связывает названия действий проекта с физическими устройствами.
Одному действию можно назначить несколько источников, а код игры читает
`pressed()`, `down()`, `released()` или аналоговое `value()` через
`mngl::input_context`:

```cpp
#include "action_map.hpp"
#include "render_window.hpp"

mngl::render_window window("My game", {1280, 720});
mngl::action_map actions;
mngl::action_presets::platformer(actions); // move_left, move_right, jump, pause
actions.bind_mouse_button("attack", mngl::mouse_button::left);
actions.bind_gamepad_button("attack", mngl::gamepad_button::rightshoulder);

mngl::input_context gameplay(actions, window.id());

while (mngl::input::continue_work())
{
	if (gameplay.pressed("jump"))
	{
		// Прыжок с клавиатуры или gamepad.
	}

	const float horizontal =
	    static_cast<float>(gameplay.down("move_right")) - static_cast<float>(gameplay.down("move_left"));
	if (gameplay.pressed("attack"))
	{
		// Атака с левой кнопки мыши или правой плечевой кнопки gamepad.
	}
}
```

Как и у физических состояний, `pressed()` и `released()` относятся к последнему
кадру. Если источник нажали и отпустили в одном кадре, оба флага могут быть
`true`, а `down()` останется `false`. Запрашивайте состояния каждый кадр, когда
важно не пропускать краткие нажатия.

Имена действий задаёт приложение. Пресеты `action_presets::platformer()` и
`action_presets::menu()` предлагают типовые привязки к клавиатуре и gamepad;
структуры `platformer_action_names` и `menu_action_names` позволяют переименовать
действия, а пустое имя пропускает соответствующую привязку. Пресеты добавляют
привязки в существующую карту и не очищают её.

Кроме кнопок можно привязывать сырые оси joystick, оси gamepad, движение и прокрутку
мыши через `mouse_axis`, давление touch и оси пера. `bind_*_axis()` принимает
масштаб, dead zone и порог `down()`: `value()` возвращает сумму привязок,
ограниченную диапазоном `[-1, 1]`, а `pressed()` / `released()` срабатывают при
пересечении порога действия. Для пиксельной дельты мыши задавайте подходящий
`scale`, например `0.01f`, чтобы преобразовать её в удобный диапазон.

`input_context` можно включать и выключать через `set_enabled()`, а область окна
менять через `set_window_id()`. ID окна `0` означает ввод из любого окна. Клавиши,
кнопки мыши, touch и pen фильтруются по окну контекста; joystick и gamepad являются
глобальными и видны в каждом активном контексте, если привязка не ограничена ID
устройства. Контексты не забирают ввод друг у друга: приложение само включает
нужные, например меню или игровой контекст.
Захват клавиатуры/мыши Dear ImGui сам по себе не выключает действия в
`input_context`; приложение управляет контекстом через `set_enabled()`.

При потере фокуса оконный контекст сбрасывает активные действия с причиной
`focus_lost`. После возврата фокуса он ждёт, пока привязанные источники станут
нейтральными, поэтому удерживаемая кнопка контроллера не включит действие сама.
При уничтожении окна активные действия получают `window_destroyed`; контекст
нужно перестать использовать вместе с окном.
Изменение `action_map` после создания контекста отражается в нём автоматически;
на кадре изменения состояние заново синхронизируется без искусственного `pressed`.
Копии `action_map` разделяют набор привязок. Пользовательское переназначение,
сериализация и сохранение настроек пока не входят в этот слой.

## Dear ImGui

`mngl::imgui_context` подключает Dear ImGui к `mngl::render_window` через SDL3 и SDL_GPU. UI рисуется отдельным GPU-проходом поверх swapchain-текстуры — не через `frame_2d` и не как набор 2D-спрайтов.

```cpp
#include "imgui_compat.hpp"
#include "input.hpp"
#include "render_window.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

mngl::render_window window("My app", {1280, 720});
mngl::imgui_context gui(window);
mngl::texture preview = window.create_texture({400, 300});
preview.clear();
preview.display();
int clicks = 0;
std::string username;
const std::regex username_pattern(R"([A-Za-z0-9_]*)");

while (mngl::input::continue_work())
{
	{
		auto frame = gui.frame();
		auto panel = frame.window("Debug");
		if (panel)
		{
			panel.text("MNGL + Dear ImGui");
			if (panel.button("Click"))
			{
				++clicks;
			}
			(void)panel.input("Username", username, username_pattern);
			panel.image(preview, {180.0f, 135.0f});
		}
	} // Завершает ImGui-кадр до window.display().

	window.clear();
	window.display();
}
```

Создавайте ImGui-кадр после `input::continue_work()` и завершайте его до `render_window::display()`: `display()` добавляет UI-проход после 2D-кадра. `input` передаёт SDL-события backend-у на главном потоке. Устаревшие агрегированные запросы ввода фильтруются по захвату ImGui; физические состояния отдельных устройств и `input_context` этого автоматически не делают. `frame_scope::input()` сообщает `wants_keyboard`, `wants_mouse` и `wants_text`; по ним приложение может временно выключить игровой `input_context` через `set_enabled()`.

Обёртки `frame_scope`, `window_scope`, `dockspace_scope`, `main_menu_bar_scope` и `menu_scope` управляют временем жизни кадра, окон, меню и dockable-панелей. Они предоставляют `window()`, `dockspace()`, `main_menu_bar()`, `menu()`, `toggle()`, `menu_item()`, `text()`, `text_wrapped()`, `text_disabled()`, `text_colored()`, `button()`, `small_button()`, `input()`, `input_text()`, `checkbox()`, `selectable()`, `context_menu()`, `radio_button()`, `bullet()`, `progress_bar()`, `slider()`, `combo()` и `image()`.

## Виджеты и компоновка через MNGL

Для обычного UI вызывайте методы MNGL scope-объектов. Интерфейс принимает `std::string_view`, `glm::vec2`, `mngl::color` и типы опций MNGL; символы, флаги и структуры Dear ImGui остаются внутри адаптера.

Фасад покрывает типичный интерфейсный код без вызовов `ImGui::`: блоки `Begin*`/`End*` становятся вложенными scope-объектами; `GetIO()` — снимком `frame.input()`; проверки клавиш и мыши — методами `frame`; `GetWindowDrawList()` — ограниченным `canvas_scope`. Для динамического текста передавайте результат `std::format()` в `text()` или `text_colored()`; для цветов используйте `mngl::color`, а не packed-цвет Dear ImGui.

```cpp
auto ui = gui.frame();
ui.dockspace_over_viewport();

bool show_tools = true;
if (auto tools = ui.window("Tools", show_tools, {.no_collapse = true}))
{
	tools.text("Tools panel");
}

if (auto menu_bar = ui.main_menu_bar())
{
	if (auto file = menu_bar.menu("File"); file && file.menu_item("Open"))
	{
		open_file();
	}
}

if (auto panel = ui.window("Tree"))
{
	panel.text_wrapped("MNGL widgets do not require Dear ImGui calls in application code.");
	panel.set_next_item_width(-1.0f);
	(void)panel.input_text("Filter", filter, "Search nodes...");

	if (auto child = panel.child("Results", {.size = {0.0f, 240.0f}, .border = true}))
	{
		if (child.collapsing_header("Visible nodes", true))
		{
			child.text_disabled("Filtered results appear here.");
		}
	}

	if (auto table = panel.table("Properties", 2,
	                             {.row_background = true, .inner_vertical_borders = true}))
	{
		table.setup_column("Property");
		table.setup_column("Value");
		table.table_headers_row();
		table.next_row();
		(void)table.next_column();
		table.text("Root");
		(void)table.next_column();
		table.text("42");
	}
}
```

`child()`, `begin_combo()`, `popup()`, `modal()`, `context_popup()`, `tab_bar()`, `tab()`, `table()`, `tooltip()`, `disabled()` и `tree_node()` возвращают scope с RAII-завершением соответствующего блока. Закрывающая операция вызывается при выходе из области, даже если дочернее окно оказалось скрыто. `last_item()`, `focused()`, `available_size()`, `cursor_screen_position()`, `metrics()` и методы прокрутки дают нужное состояние без доступа к ImGui IO или style-структурам.

Окно можно связать с переменной видимости через `frame.window(title, open, options)` или `dockspace.window(title, open, options)`. `window_options` задаёт типичные настройки заголовка, изменения размера и позиции, прокрутки, фона, меню, навигации и docking без битовых флагов ImGui.

Для интерактивной области с пользовательской отрисовкой используйте `canvas()`. Он создаёт hit target, сообщает состояние мыши и ограничивает рисунок прямоугольником canvas:

```cpp
auto canvas = panel.canvas("TreeCanvas", panel.available_size(),
	                        {.left_button = true, .middle_button = true, .right_button = true});
if (canvas)
{
	canvas.fill_rectangle({{8.0f, 8.0f}, {96.0f, 32.0f}}, mngl::color(0.12f, 0.16f, 0.22f));
	canvas.draw_rectangle({{8.0f, 8.0f}, {96.0f, 32.0f}}, mngl::color(0.4f, 0.6f, 0.9f));
	canvas.text({16.0f, 16.0f}, "root", mngl::color(1.0f));
	if (canvas.clicked(mngl::mouse_button::middle))
	{
		// Начать перемещение canvas.
	}
}
```

`canvas_scope` рисует изображения MNGL, залитые и обведённые прямоугольники, линии и текст через внутренний ImGui draw list. `frame_scope::key_pressed()`, `mouse_down()`, `mouse_clicked()`, `mouse_double_clicked()`, `mouse_dragging()`, `input()`, `main_viewport()`, `set_next_window_position()` и `set_next_window_size()` покрывают состояние клавиатуры, мыши и главного viewport типами MNGL.

## Виджеты-объекты с собственным состоянием

Если состояние удобнее хранить вместе с элементом интерфейса, используйте `mngl::imgui_compat::widgets`. Каждый объект создаётся один раз, хранит значение и настройки, а `draw()` вызывается для нужного окна каждый кадр. Это дополнительный слой: прямые методы `window_scope` остаются удобны для коротких и динамических UI.

```cpp
namespace ui_widget = mngl::imgui_compat::widgets;

ui_widget::button save{"Save"};
ui_widget::checkbox show_grid{"Show grid", true};
ui_widget::slider<float> zoom{"Zoom", 1.0f, 0.25f, 4.0f};
ui_widget::text_input filter{"Filter", "", "Search nodes..."};
ui_widget::numeric_input<int> visible_layers{"Layers", 4};
ui_widget::combo<std::string> renderer{"Renderer", {"Default", "Software"}};
ui_widget::image preview_image{{180.0f, 135.0f}};

while (mngl::input::continue_work())
{
	{
		auto frame = gui.frame();
		if (auto panel = frame.window("Tools"))
		{
			(void)save.draw(panel);
			if (save) save_document();
			(void)show_grid.draw(panel);
			(void)zoom.draw(panel);
			(void)filter.draw(panel);
			(void)visible_layers.draw(panel);
			(void)renderer.draw(panel);
			preview_image.draw(panel, preview);
		}
	}
	window.display();
}
```

`checkbox::value()`, `slider::value()`, `text_input::value()`, `numeric_input::value()` и `combo<T>::selected()` читают или меняют значения; `selectable` хранит состояние выбора, переключает его при нажатии и возвращает факт нажатия. `slider<T>` принимает скаляр или `glm`-вектор, а `combo<T>` хранит элементы и использует тот же протокол преобразования подписей, что и `combo(label, range)`. `text_input::set_multiline_size()` включает многострочный режим; regex-проверка применяется к однострочному вводу. `button` можно проверить в `if` после `draw()`: `operator bool()` сообщает результат последнего вызова `draw()`. `set_enabled()` отключает controls, а `radio_button<T>` связывает вариант с общим значением выбора; `context_menu` возвращает индекс выбранной команды и рисуется сразу после элемента, к которому привязан. `image::draw(panel, texture)` получает текстуру на каждый вызов и не удерживает её. Объекты не хранят ImGui-контекст и могут рисоваться в разных окнах, пока переданный `window_scope` активен.

Классовый слой покрывает элементы и контейнеры всего текущего MNGL UI: `text`, `button`, `checkbox`, `selectable`, `bullet`, `radio_button<T>`, `combo<T>`, `combo_popup`, `context_menu`, `image`, `text_input`, `numeric_input<T>`, `slider<T>`, `progress_bar`, `collapsing_header`, `tree_node`, `window`, `child`, `popup`, `modal`, `context_popup`, `tab_bar`/`tab`, `table`, `tooltip`, `disabled`, `dockspace`, `main_menu_bar`/`menu`/`menu_item`, `font`, `canvas`, `style_color` и `id_block`. Контейнеры получают содержимое через callback. Scope остаётся локальным внутри `draw()`, поэтому каждый ImGui-блок гарантированно закрывается при выходе, включая исключения из callback. Одноразовые операции компоновки, измерения, фокуса и прокрутки остаются методами типизированного `drawing_context`:

```cpp
ui_widget::window tools{"Tools"};
ui_widget::main_menu_bar menu_bar;
ui_widget::menu file_menu{"File"};
ui_widget::menu_item save_item{"Save", "Ctrl+S"};
ui_widget::child results{"Results", {.size = {0.0f, 240.0f}, .border = true}};
ui_widget::tree_node root{"Root", {.default_open = true}};
ui_widget::table properties{"Properties", 2, {.row_background = true}};
ui_widget::collapsing_header advanced{"Advanced"};
ui_widget::combo_popup recent_file{"Recent file", "Choose a file"};

while (mngl::input::continue_work())
{
	{
		auto frame = gui.frame();
		(void)menu_bar.draw(frame, [&](auto& bar)
		{
			(void)file_menu.draw(bar, [&](auto& file)
			{
				if (save_item.draw(file)) save_document();
			});
		});

		(void)tools.draw(frame, [&](auto& panel)
		{
			(void)results.draw(panel, [&](auto& list)
			{
				(void)root.draw(list, [&](auto& node)
				{
					node.text("Child node");
				});
			});

			(void)properties.draw(panel, [&](auto& table)
			{
				table.setup_column("Property");
				table.setup_column("Value");
				table.table_headers_row();
				table.next_row();
				(void)table.next_column();
				table.text("Visible");
				(void)table.next_column();
				table.text("True");
			});

			(void)advanced.draw(panel, [&](auto& contents)
			{
				contents.text("Advanced settings");
			});

			(void)recent_file.draw(panel, [&](auto& contents)
			{
				if (contents.selectable("scene.mngl")) open_recent_file("scene.mngl");
				if (contents.selectable("project.mngl")) open_recent_file("project.mngl");
			});
		});
	}
	window.display();
}
```

`collapsing_header::draw()` возвращает состояние раскрытия; с callback он запускает содержимое только пока заголовок открыт. `combo_popup` хранит подпись предпросмотра и позволяет собрать пользовательское содержимое выпадающего списка. `combo<T>` остаётся готовым вариантом с типизированным набором значений.

`key` включает стандартные клавиши, цифры, буквы, F1–F24, пунктуацию, keypad и клавиши меню браузера; `frame.input()` также возвращает модификаторы, захват UI, необязательную позицию курсора, его смещение, прокрутку, `delta_time` и масштаб framebuffer. Пример обработки сочетания клавиш с учётом ввода текста:

```cpp
const auto state = ui.input();
if (ui.key_pressed(mngl::imgui_compat::key::s) && state.control && !state.wants_text)
{
	save_document();
}
```

Чтобы выводить весь ImGui workspace в одну offscreen-текстуру, используйте `mngl::resizable_texture`: она обновляет backing-текстуру до `window.pixel_size()` при обращении к `target()`, а `as_sprite()` подгоняет размер спрайта к логической области окна. Обычные `mngl::texture` при изменении окна остаются прежнего размера. Для HiDPI включите `window_flags.high_pixel_density`: backing-текстура создаётся в физических пикселях, а ImGui сохраняет логический размер элементов, учитывая `DisplayFramebufferScale` от SDL. Текстура должна оставаться живой до `window.display()`:

```cpp
mngl::window::flags window_flags;
window_flags.high_pixel_density = true;
mngl::render_window window("My app", {1280, 720}, window_flags);
mngl::resizable_texture workspace(window);
mngl::imgui_context gui(window);
window.enable_hit_testing();

while (mngl::input::continue_work())
{
	if (window.pixel_size().x == 0 || window.pixel_size().y == 0)
	{
		continue;
	}

	{
		auto frame = gui.frame(workspace.target());
		auto host = frame.window("Workspace");
		auto docks = host.dockspace("MainWorkspace");
		auto panel = docks.window("Controls");
		if (panel)
		{
			panel.text("The complete workspace is one texture.");
		}
	}

	window.clear(workspace.target(), mngl::color{0.0f, 0.0f, 0.0f, 0.0f});
	window.display();

	window.clear();
	window.draw(workspace.as_sprite());
	window.display();
}
```

Для маршрутизации мыши включите hit-testing на `render_window` до первого кадра (`window.enable_hit_testing()`). `gui.frame(workspace.target())` находит поверхность в hit-test-пути верхнего спрайта и преобразует курсор в пиксели текстуры, учитывая размер, поворот и UV-область спрайта. Наведение вне поверхности по-прежнему передаётся сцене, но начатое на ней перетаскивание продолжает получать координаты за её границами; продолжение за границами OS-окна зависит от поддержки захвата мыши SDL платформой. Клавиатурные и текстовые события по-прежнему приходят через SDL backend.

Демо `MNGL.exe` служит ручным стендом: окно можно менять по размеру, режимы в панели «Управление» проверяют перенос, масштаб, поворот и UV crop поверхности, а `F1` скрывает её, чтобы проверить hit-test кнопки сцены под ней. Оно выводит логический и физический размеры окна и обеих offscreen-текстур. Прозрачные участки спрайта пока считаются попаданием; начав перетаскивание на поверхности, его можно продолжить за её границами.

Docking включён в vendored-версии Dear ImGui. `window_scope::dockspace(id)` создаёт область внутри текущего окна, а `dockspace_scope::window(title, open)` привязывает панель к ней при первом открытии. Панели стартуют вкладками в общей области; перетаскивайте вкладку к краю, чтобы разделить её, или выбирайте вкладку, чтобы переключиться. Панели можно отсоединить и свободно перемещать внутри поверхности ImGui; multi-viewport не включён, поэтому отдельные OS-окна не создаются. Закрытые панели можно вернуть из меню «Вид»: `menu_scope::toggle(label, bool&)` синхронизирует пункт меню с состоянием панели.

Раскладка сохраняется в `MNGL_imgui.ini` относительно текущей рабочей папки и восстанавливается при следующем запуске. ID dockspace и названия панелей должны оставаться стабильными; используйте суффикс ImGui `###id`, если хотите поменять отображаемый заголовок без смены ID. Вызывайте `dockspace()` каждый кадр, даже если окно-хост свернуто: так ImGui сохраняет связь dockspace с панелями.

```cpp
bool show_hierarchy = true;
bool show_inspector = true;

auto frame = gui.frame();
{
	auto menu_bar = frame.main_menu_bar();
	if (menu_bar)
	{
		auto view = menu_bar.menu("View");
		if (view)
		{
			(void)view.toggle("Hierarchy", show_hierarchy);
			(void)view.toggle("Inspector", show_inspector);
		}
	}
}
auto host = frame.window("Editor");
auto docks = host.dockspace("EditorDockspace");
{
	auto hierarchy = docks.window("Hierarchy###hierarchy", show_hierarchy);
	if (hierarchy)
	{
		hierarchy.text("Scene objects");
	}
}
{
	auto inspector = docks.window("Inspector###inspector", show_inspector);
	if (inspector)
	{
		inspector.text("Selected object properties");
	}
}
```

`input(label, std::string&, regex)` редактирует `std::string` без пользовательского `char*`-буфера и возвращает `true`, если новое значение принято. Regex проверяет всю строку (`std::regex_match`): изменение принимается только если результат целиком совпадает с выражением, иначе текст сразу возвращается к последнему совпавшему значению. Начальное значение тоже должно совпадать. Поэтому выражение должно допускать промежуточные состояния, через которые проходит набор; например, `R"([A-Za-z0-9_]*)"` разрешает пустую строку и любой ввод из ASCII-букв, цифр и `_`. При использовании `+` или обязательной минимальной длины соответствующий префикс нельзя набрать посимвольно. Передавайте заранее созданный `std::regex`, а не конструируйте его заново в цикле кадров.

Числовой `input(label, value)` принимает значение по ссылке: целые стандартные типы размером 1, 2, 4 или 8 байт, а также `float` и `double`. Он показывает числовое поле ImGui и возвращает `true`, когда значение изменилось; regex к числовому вводу не применяется. Для `bool` `input(label, value)` отображается как checkbox; можно также явно вызвать `checkbox(label, value)`. Например:

```cpp
int sample_count = 8;
float ui_scale = 1.0f;
double precise_value = 0.125;
bool show_texture = true;

panel.input("Sample count", sample_count);
panel.input("UI scale", ui_scale);
panel.input("Precise value", precise_value);
panel.checkbox("Show texture", show_texture);
```

Все эти функции работают с ссылками на значения — передавать указатели из пользовательского кода не нужно.

`text(value)` выводит целые значения, `float`, `double` и `bool` без ручного форматирования: целые отображаются десятичным числом, `bool` — как `0` или `1`, вещественные форматируются через `std::format`. Для текста остаётся базовый `text(std::string_view)`. Если нужна подпись, точность или другой формат, сформируйте строку сами и передайте её в `text()`:

```cpp
panel.text(sample_count);
panel.text(show_texture);
panel.text(std::format("Scale: {:.2f}", ui_scale));
```

`button(value)` использует то же отображение значения как подпись кнопки; возвращаемое значение по-прежнему означает, что кнопку нажали. Для собственной подписи передайте строку, например `panel.button(std::format("Count: {}", sample_count))`.

`progress_bar(fraction)` показывает заполнение от `0.0f` до `1.0f`; другое значение не из этого диапазона или не конечное приводит к `std::invalid_argument`. Перегрузка с `overlay` выводит свою строку поверх полосы. `selectable(label, selected)` возвращает `true` при нажатии, но не меняет `selected` автоматически — состояние обновляет вызывающий код. `checkbox(label, bool&)` уже меняет значение и возвращает `true`, если оно изменилось. Радиокнопки можно связать общим значением, которое выбранная кнопка обновляет при нажатии:

```cpp
float loading_progress = 0.68f;
bool row_selected = false;
int quality = 1;

panel.progress_bar(loading_progress);
panel.progress_bar(loading_progress, "Loading");
if (panel.selectable("Selectable row", row_selected))
{
	row_selected = !row_selected;
}
panel.bullet("A bullet with text");

panel.radio_button("Low", quality, 0);
panel.radio_button("Medium", quality, 1);
panel.radio_button("High", quality, 2);
```

`bullet()` без аргументов рисует только маркер и оставляет курсор на той же строке; `bullet(text)` сразу выводит рядом и текст. Подписи checkbox, selectable, radio button и bullet принимают также числовые значения и `bool`, используя тот же вывод, что `text(value)`.

`slider(label, value, min, max)` принимает те же числовые типы, что и `input()`, и возвращает `true`, если пользователь изменил значение. Границы должны образовывать непустой диапазон, а начальное значение — находиться внутри него; иначе метод бросает `std::invalid_argument`. Например:

```cpp
panel.slider("Sample count", sample_count, 0, 100);
panel.slider("UI scale", ui_scale, 0.25f, 2.0f);
panel.slider(std::format("Scale {:.2f}", ui_scale), ui_scale, 0.25f, 2.0f);
```

Для используемых в MNGL векторов GLM тот же вызов доступен для `glm::vec2`, `glm::vec3`, `glm::vec4` и целочисленных вариантов: все компоненты получают общий диапазон `[min, max]`, а метод возвращает `true`, если изменилась хотя бы одна. Например:

```cpp
glm::vec2 position{0.2f, 0.5f};
glm::ivec2 grid_position{2, 4};

panel.slider("Position", position, -1.0f, 1.0f);
panel.slider("Grid position", grid_position, -20, 20);
```

`combo(label, values)` строит выпадающий список из конечного forward-range. Элементы должны преобразовываться в `std::string_view`/строку или иметь доступный через ADL либо `std::to_string` вызов `to_string(value)`. Для собственного типа объявите `to_string(const T&)` в его ассоциированном namespace. Результат — `std::optional`, возвращаемый каждый кадр: `nullopt` означает, что выбор отсутствует, а пункт `(none)` позволяет его сбросить. Выбор сохраняется внутри ImGui по ID виджета, поэтому соблюдайте уникальность подписей (или используйте `##id`, как в ImGui). Для lvalue-контейнера или borrowed-range возвращается итератор (указатель для массива); для временного контейнера — копия элемента, если она копируема, иначе индекс. Итератор используйте, пока контейнер не изменён; не сохраняйте его через операции, которые могут инвалидировать итераторы.

```cpp
const std::vector<std::string> modes{"Solid", "Wireframe", "Points"};
if (const auto selected = panel.combo("Render mode", modes))
{
	panel.text(**selected); // optional<iterator>
}

if (const auto quality = panel.combo("Quality", std::initializer_list<std::string>{"Low", "Medium", "High"}))
{
	panel.text(*quality); // optional<string> для временного списка
}
```

`context_menu(values)` открывает контекстное меню по правому клику на последнем ImGui-элементе. Вызывайте его сразу после виджета с собственным ID, например `selectable()` или `button()`. Список принимает те же диапазоны и типы подписей, что и `combo()`, но результат появляется только в кадре, когда пользователь выбрал пункт; в остальных случаях возвращается пустой `std::optional`. Для lvalue-контейнера результат содержит итератор, для временного — копию элемента, если она доступна, иначе индекс. Выбор не сохраняется между кадрами: это удобно для команд меню.

```cpp
const std::vector<std::string> actions{"Rename", "Delete"};

(void)panel.selectable("Object", object_selected);
if (const auto action = panel.context_menu(actions))
{
	if (**action == "Rename")
	{
		// Переименовать объект.
	}
	else if (**action == "Delete")
	{
		// Удалить объект.
	}
}
```

`context_menu()` использует ID предыдущего виджета, поэтому повторяющиеся элементы в цикле должны иметь уникальные ImGui ID (например, задаваемые через `PushID`). Для текстовых элементов без ID используйте виджет с ID, например `selectable()`.

`image()` показывает всю `mngl::texture` заданного размера в логических координатах ImGui. Текстура должна быть sampleable, принадлежать тому же GPU-устройству, не рендериться в этот момент и оставаться живой до `window.display()`.

`imgui_compat.hpp` содержит header-only код MNGL для инициализации backend-ов, стиля и загрузки шрифтов. Исходники самого Dear ImGui и SDL3/SDL_GPU backend-ов по-прежнему компилируются как отдельные `.cpp`.

По умолчанию используется тёмная палитра в духе Visual Studio. Встроенный масштабируемый векторный шрифт ImGui размером 18 px рассчитан на базовую латиницу; внешние TTF/OTF загружаются с диапазонами латиницы и кириллицы. Чтобы задать шрифты и начертания, передайте пути в конфигурации:

```cpp
mngl::imgui_compat::font_config fonts;
fonts.regular.file = "assets/fonts/Regular.ttf";
fonts.bold.file = "assets/fonts/Bold.ttf";
fonts.monospace.file = "assets/fonts/Mono.ttf";
fonts.regular.size_pixels = 18.0f;

mngl::render_window window("My app", {1280, 720});
mngl::imgui_context gui(window, fonts);

while (mngl::input::continue_work())
{
	{
		auto frame = gui.frame();
		auto panel = frame.window("Настройки");
		if (panel)
		{
			auto title = panel.use_font(mngl::imgui_compat::font_role::bold, 24.0f);
			panel.text("Заголовок");
		}
	}

	window.clear();
	window.display();
}
```

Пути шрифтов трактуются относительно текущей рабочей папки процесса; пустой путь у начертания использует regular-шрифт. Размеры задаются в логических пикселях и могут меняться через `use_font()`.

### Масштабирование HiDPI и пользовательский масштаб

MNGL автоматически обрабатывает экраны с высоким разрешением (HiDPI / 4K):
- `window.display_scale()` возвращает системный масштаб экрана (например, `1.5` для 150% или `2.0` для 4K/200%).
- `mngl::imgui_context` на лету отслеживает изменение масштаба дисплея и применяет масштаб к геометрии виджетов через `style.ScaleAllSizes(scale)`, а к шрифтам — через `style.FontScaleDpi = scale`.
- Встроенные векторные шрифты Dear ImGui 1.92 выпекаются на GPU в физическом разрешении (`FontSizeBase * FontScaleDpi`), сохраняя безупречную чёткость без размытия.
- Координаты мыши и элементов интерфейса транслируются 1:1 в физические пиксели поверхности/текстуры.
- Метод `gui.set_user_scale(float)` позволяет задать дополнительный пользовательский коэффициент масштабирования интерфейса (доступен через `gui.user_scale()` и `gui.effective_scale()`).

Контекст должен жить не дольше своего `render_window`; поддерживается один активный контекст, и кадры создаются на SDL main thread.
