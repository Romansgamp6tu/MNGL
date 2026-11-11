# Ввод на практике: действия, пауза и переназначение

Для «прыжка», «выстрела» и «подтверждения» используйте именованные действия. Код игры будет знать имя действия, а конкретная клавиша останется в `action_map`. Физические устройства нужны, когда интерфейс выбирает конкретную мышь/контроллер или записывает новую привязку. Полное описание устройств находится в [справочном руководстве по вводу](windows-and-input.md).

## Нажатие и удержание решают разные задачи

| Что нужно сделать | Что опрашивать |
|---|---|
| Один прыжок за нажатие | `context.pressed("jump")` |
| Двигаться всё время, пока удерживают кнопку | `context.down("move_left")` каждый кадр |
| Завершить перетаскивание | `context.released("drag")` |
| Прочитать аналоговую величину | `context.value("move_x")` |
| Понять причину сброса | `context.release_reason("drag")` |

`pressed()` и `released()` действуют один кадр. Быстрое нажатие и отпускание внутри одного кадра может дать оба флага при `down() == false`. Удержание в `pressed()` не превращается в движение: это однократный переход, поэтому для движения нужен `down()` и умножение скорости на `input::delta_time()` в секундах.

## Рабочая программа: движение и пауза

Пример не требует изображений или GPU. A/D меняют условную координату, Space печатает «прыжок», Escape переключает паузу. Координата показывается в заголовке окна.

```cpp
#include <MNGL/action_map.hpp>
#include <MNGL/input.hpp>
#include <MNGL/runtime.hpp>
#include <MNGL/window.hpp>
#include <exception>
#include <iostream>
#include <string>

int main()
{
	try
	{
		mngl::runtime::ensure();
		mngl::window window("Actions", {800, 600});

		mngl::action_map controls;
		controls.bind_key("move_left", mngl::scancode::a);
		controls.bind_key("move_right", mngl::scancode::d);
		controls.bind_key("jump", mngl::scancode::space);
		controls.bind_key("pause", mngl::scancode::escape);

		mngl::input_context global_controls(controls, window.id());
		mngl::input_context gameplay(controls, window.id());
		double player_x = 0.0;
		bool paused = false;

		while (mngl::input::next_frame())
		{
			if (global_controls.pressed("pause"))
			{
				paused = !paused;
				gameplay.set_enabled(!paused);
			}

			const double direction = static_cast<double>(gameplay.down("move_right")) -
			                         static_cast<double>(gameplay.down("move_left"));
			player_x += direction * 100.0 * mngl::input::delta_time();
			if (gameplay.pressed("jump"))
				std::cout << "Jump\n";

			window.title(std::string(paused ? "Paused, x=" : "Playing, x=") +
			             std::to_string(player_x));
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
```

Оба контекста читают одну карту, но включаются независимо. Команда паузы остаётся в активном `global_controls`, поэтому Escape может вернуть игру из паузы. Если выключить единственный контекст вместе с этой командой, прочитать её для возобновления уже не получится.

Контексты не забирают события друг у друга. Если опрашивать `jump` в обоих активных контекстах, оба его увидят. Приложение само выбирает, какие команды относятся к меню, игре и глобальным сочетаниям. У Dear ImGui захват клавиатуры/мыши не выключает `input_context` автоматически: учитывайте состояние UI и управляйте `set_enabled()`.

## Несколько источников одного действия

`bind_*` добавляет привязки. Например, после создания `controls` внутри функции:

```cpp
controls.bind_key("jump", mngl::scancode::space);
controls.bind_gamepad_button("jump", mngl::gamepad_button::a);
controls.bind_mouse_button("attack", mngl::mouse_button::left);
```

Код игры по-прежнему читает `gameplay.pressed("jump")`. Чтобы заменить клавишу, сначала уберите прежние источники действия:

```cpp
controls.clear_action("jump");
controls.bind_key("jump", mngl::scancode::w);
controls.bind_gamepad_button("jump", mngl::gamepad_button::a);
```

Если не вызвать `clear_action()`, Space и W будут работать одновременно. Правки общей карты видны существующим контекстам. На кадре изменения контекст синхронизируется без искусственного `pressed()` от уже удерживаемого источника.

Копии `action_map` разделяют storage: `auto copy = controls;` не создаёт независимый набор настроек. Для отдельной карты меню создайте новый `action_map` и заполните его отдельно.

## Сохранение настроек и безопасный старт

Профиль содержит привязки, масштабы, dead zone и пороги, но не состояние контекста, ID окна или состояние «пауза». Это версионированный формат MNGL, а не JSON. Приложение выбирает имя файла; подходящее место — `paths::user_data_directory()`.

Ниже — функции для стартовой загрузки. Повреждённый файл, неизвестная версия, отсутствие нужных действий или ошибка доступа оставляют рабочие настройки по умолчанию. Имена обязательных действий определяет приложение.

```cpp
#include <MNGL/action_map.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string_view>

mngl::action_map default_controls()
{
	mngl::action_map result;
	result.bind_key("move_left", mngl::scancode::a);
	result.bind_key("move_right", mngl::scancode::d);
	result.bind_key("jump", mngl::scancode::space);
	result.bind_key("pause", mngl::scancode::escape);
	return result;
}

mngl::action_map load_controls(const std::filesystem::path& path)
{
	auto defaults = default_controls();
	try
	{
		if (!std::filesystem::exists(path))
			return defaults;

		mngl::action_map candidate; // Независимая карта; не копия defaults.
		candidate.load_profile(path);
		constexpr std::string_view required[] = {"move_left", "move_right", "jump", "pause"};
		for (const auto name : required)
		{
			if (candidate.binding_count(name) == 0)
				throw std::runtime_error("Profile is missing a required action");
		}
		return candidate;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Using default controls: " << error.what() << '\n';
		return defaults;
	}
}
```

В `main()`, после инициализации runtime и до создания контекстов, замените заполнение карты следующими строками; добавьте `<MNGL/paths.hpp>`:

```cpp
const auto profile_path = mngl::paths::user_data_directory() / "controls.mnglprofile";
auto controls = load_controls(profile_path);
mngl::input_context gameplay(controls, window.id());
```

После подтверждённого пользователем переназначения сохраните уже изменённую общую карту. Ошибку записи сообщите в UI; не показывайте «настройки сохранены», пока вызов не завершился успешно:

```cpp
try
{
	controls.save_profile(profile_path);
	std::cout << "Controls saved\n";
}
catch (const std::exception& error)
{
	std::cerr << "Cannot save controls: " << error.what() << '\n';
}
```

`load_profile()` проверяет файл полностью перед заменой привязок. Поэтому ошибка синтаксиса не портит текущую карту. Проверку обязательных имён в примере делают на отдельном кандидате до создания контекстов: присваивание новой карты уже существующей переменной не переводит ранее созданные контексты на другое storage.

## Устройства, фокус и горячее подключение

Обычно сохраняемая привязка не должна содержать конкретный SDL device ID. Пропущенный аргумент устройства означает «любое устройство этого типа»; после переподключения или перезапуска SDL ID может измениться. Индекс из списка устройств тоже не является постоянным ID.

`input::keyboards()`, `input::mice()` и списки gamepad/joystick — снимки. Полученный объект устройства привязан к его ID, но `connected()` может позже стать `false`. Снова перечисляйте устройства для выбора нового подключения. Если клавиатур нет, одиночный `input::keyboard()` бросит `std::out_of_range`; при допустимом отсутствии оборудования сначала проверьте список.

Контекст с ID окна фильтрует оконные источники и сбрасывает действия при потере фокуса/уничтожении окна. Joystick/gamepad глобальны, но неактивный оконный контекст всё равно не должен выполнять игровую команду. После возврата фокуса он ждёт нейтрального состояния источников. Для сброса перетаскивания или заряженной атаки смотрите `release_reason()`: отпускание из-за `focus_lost` может требовать другой реакции, чем обычный `physical_release`.

Следующий шаг — [полный перечень устройств и методов привязки](windows-and-input.md) или [события](events.md), если нужна доставка конкретного сообщения.
