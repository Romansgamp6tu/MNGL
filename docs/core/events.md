# События: подписка, доставка и время жизни

`<MNGL/event.hpp>` предоставляет типизированные события SDL и шину событий приложения. Подписка отвечает на вопрос «что произошло»: окно изменило размер, устройство отключилось, приложение получило файл. Для игрового движения обычно удобнее покадровые состояния из [ввода и действий](input-recipes.md): удержание клавиши нельзя заменить единственным событием нажатия.

## Первая подписка: размер конкретного окна

Это полная программа. Изменяйте размер окна мышью: в консоли появится новый размер и число уведомлений.

```cpp
#include <MNGL/event.hpp>
#include <MNGL/input.hpp>
#include <MNGL/runtime.hpp>
#include <MNGL/window.hpp>
#include <exception>
#include <iostream>

int main()
{
	try
	{
		mngl::runtime::ensure();
		mngl::window::flags flags{};
		flags.resizable = true;
		mngl::window window("Resize me", {800, 600}, flags);
		int resize_count = 0;
		auto resized = mngl::event_manager::on<mngl::window_resized>(
		    [id = window.id(), &resize_count](const mngl::window_resized& event)
		    {
			    if (event.window_id != id)
				    return;
			    ++resize_count;
			    std::cout << event.size.x << " x " << event.size.y
			              << "; notifications: " << resize_count << '\n';
		    });

		while (mngl::input::next_frame())
		{
			// События уже доставлены; здесь обновляется приложение.
		}
	} // resized уничтожается раньше resize_count и window.
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
```

`on<T>()` выбирает структуру события, а параметр lambda — её данные. Оконные события содержат `window_id`; подписка не ограничивается одним окном автоматически. `window_resized::size` описывает размер окна, а `window_pixel_size_changed` относится к размеру в пикселях. Для HiDPI это разные величины; см. [координаты и view](../2d/coordinates-and-view.md).

`input::next_frame()` прокачивает очередь SDL и доставляет события в потоке этого вызова. `mngl::poll_events()` тоже доставляет ожидающие события, но не начинает новый кадр состояния ввода. Для обычного игрового цикла достаточно одного `next_frame()` на кадр.

## Соединение — это сама подписка

Результат `on()` — move-only `mngl::event_connection`. Его нужно сохранить. Если написать только `event_manager::on<T>(callback);`, временное соединение уничтожится в конце выражения и callback сразу отключится.

| Операция | Результат |
|---|---|
| Хранить соединение в локальной переменной | Подписка живёт до выхода из её области |
| Хранить соединение в поле объекта | Подписка живёт вместе с владельцем |
| `connection.disconnect()` | Отключить подписку раньше |
| `connection.connected()` | Проверить, подключено ли соединение |
| Переместить соединение | Передать владение подпиской; копирование запрещено |

Временный экран может отключиться явно. Фрагмент выполняется внутри функции; `window` и `std::cout` доступны из предыдущего примера:

```cpp
auto connection = mngl::event_manager::on<mngl::window_focus_lost>(
    [id = window.id()](const mngl::window_focus_lost& event)
    {
	    if (event.window_id == id)
		    std::cout << "Window lost focus\n";
    });

// Этот экран больше не интересуется фокусом.
connection.disconnect();
```

Callback может отключить собственную подписку. Если отключение вызывается из другого потока, оно ждёт уже выполняющийся callback. При захвате `[this]` соединение должно отключиться до разрушения данных, к которым обращается callback. Перемещение объекта с такой lambda само по себе не перенаправляет сохранённый `this` на новый адрес.

## События своего приложения

Собственный тип наследуется от `mngl::event_base`. Доставка выбирается по C++ типу `T`; отдельная регистрация числового ID не нужна. Это самостоятельная консольная программа с обычным target `MNGL::MNGL`:

```cpp
#include <MNGL/event.hpp>
#include <MNGL/runtime.hpp>
#include <iostream>

struct score_changed : mngl::event_base
{
	int score{};
};

int main()
{
	mngl::runtime::ensure();
	auto connection = mngl::event_manager::on<score_changed>(
	    [](const score_changed& event)
	    { std::cout << "Score: " << event.score << '\n'; });

	score_changed changed{};
	changed.score = 100;
	mngl::event_manager::emit(changed);
	// К этой строке callback уже отработал.
}
```

Обычный вызов `emit()` синхронный: подписчики выполняются в потоке вызывающего кода. Это не очередь задач для главного потока. Если рабочий поток вызывает `emit()`, его callbacks тоже работают в этом рабочем потоке; операции с окном и GPU следует передать в цикл приложения отдельно.

## Где и когда выполняется callback

SDL watcher может сработать в потоке, который добавил SDL-событие. Watcher MNGL только сохраняет типизированную копию. Пользовательские подписчики получают её позже при `input::next_frame()` или `poll_events()` в потоке прокачки. Исключение callback проходит через этот обычный C++ вызов; оно не пересекает C callback SDL.

Событие передаётся как `const T&`. Ссылка и указатели на её поля действуют только во время callback. Если сообщение нужно обработать позже, скопируйте нужные значения в собственное состояние или очередь. В callback не стоит читать большой файл или ждать сеть: это задерживает весь цикл доставки.

Типы оконных, дисплейных, клавиатурных, мышиных, joystick/gamepad, touch/pen, drag-and-drop и других событий доступны через `event.hpp`. Например, `clipboard_update` содержит MIME-типы буфера, а `gamepad_removed` позволяет обновить интерфейс выбора контроллера. Физическое состояние отключённого устройства и release reasons описаны в [руководстве по вводу](windows-and-input.md).

## Если обработчик не вызывается

Сначала проверьте, сохранён ли `event_connection` и возвращает ли `connected()` значение `true`. Затем убедитесь, что цикл вызывает `input::next_frame()` или `poll_events()`, выбран нужный тип события и фильтр `window_id` сравнивает ID нужного окна. Если lambda захватила ссылку, проверьте срок жизни объекта. Эти проверки полезнее, чем добавление второго независимого SDL polling loop.
