# Аудио: эффекты, музыка и живые источники

Заголовок `<MNGL/audio.hpp>` предоставляет `mngl::audio` поверх SDL3_mixer.
`sound` хранит декодированный эффект в памяти, `music` читает длинный источник
во время воспроизведения. Каждый вызов `play()` или `try_play()` создаёт независимый голос
и возвращает `voice_handle`; управление одинаково для обоих типов ресурсов.

<a id="sdl3-mixer"></a>

## Подключение SDL3_mixer

SDL3_mixer версии `release-3.2.4` собирается из submodule
`third_party/SDL_mixer`. По умолчанию включены встроенные декодеры SDL3_mixer
для распространённых форматов; внешние декодеры можно включить CMake options
микшера. `MNGL::MNGL` передаёт динамическую библиотеку микшера потребителю,
а development-пакет содержит SDL3_mixer, её CMake config и лицензии микшера
и встроенных декодеров.
Приложению на CMake достаточно обычного подключения MNGL:

```cmake
find_package(MNGL CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE MNGL::MNGL)
```

При сборке репозитория клонируйте MNGL рекурсивно, чтобы получить SDL3_mixer и
его встроенные декодеры. На Windows DLL из `bin/` должны быть доступны при запуске;
runtime-пакет MNGL уже включает SDL3_mixer. Форматы определяются декодерами,
включёнными при сборке. CMake options SDL3_mixer можно переопределить при сборке
MNGL, если приложению нужен другой набор декодеров.

Без использования `audio.hpp` приложению не нужны символы микшера MNGL.
Полностью отключить соответствующие слои можно defines из таблицы ниже.

## Короткий синхронный API

Обычные операции возвращают ресурс напрямую и бросают `audio_error` при ошибке.
`audio_error::code()` сохраняет код backend failure. Примеры выше с `try_` показывают
вариант с явным `std::expected`; он доступен для загрузки, воспроизведения и управления.

```cpp
auto click = mngl::audio::load_sound("assets/click.wav");
auto voice = mngl::audio::play(click);
mngl::audio::set_master_volume(0.8f);
mngl::audio::stop(voice);
```

## Запуск и время жизни

Все операции `mngl::audio` выполняйте в основном потоке, кроме
`audio_stream::write()`: он потокобезопасен и подходит для producer-потока.
Создавать внутренний engine вручную не нужно: первый вызов загрузки или воспроизведения лениво создаёт
один микшер на системном устройстве воспроизведения по умолчанию.
Ошибка запуска бросается как `audio_error`, а в `try_` варианте возвращается
в `std::expected`; следующий вызов может
повторить попытку создания.

Аудио работает независимо от `input::next_frame()`. Функции `audio.update()`
нет. Внутренний engine останавливает голоса и освобождает SDL3_mixer до
`SDL_Quit()` при завершении программы. Не вызывайте `SDL_Quit()` вручную,
пока используете runtime MNGL. Потеря фокуса окна сама по себе не ставит
голоса на паузу — это решение приложения.

## Загрузка и воспроизведение эффекта

`try_load_sound(path)` принимает UTF-8 путь и предварительно декодирует эффект
в память. Относительный путь разрешается от текущего рабочего каталога.
Ниже — программа, воспроизводящая эффект по пробелу:

```cpp
#include <MNGL/audio.hpp>
#include <MNGL/input.hpp>
#include <MNGL/window.hpp>
#include <exception>
#include <iostream>
#include <string>

int main()
{
	try
	{
		mngl::runtime::ensure();
		mngl::window window("sound", {800, 600});
		auto sound = mngl::audio::try_load_sound("assets/click.wav");
		if (!sound)
		{
			std::cerr << sound.error().message << '\n';
			return 1;
		}

		while (mngl::input::next_frame())
		{
			for (const auto& keyboard : mngl::input::keyboards())
			{
				if (keyboard.pressed(mngl::scancode::space))
				{
					auto voice = mngl::audio::try_play(*sound);
					if (!voice)
					{
						std::cerr << voice.error().message << '\n';
					}
				}
			}
		}
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
```

Один `sound` можно запускать несколько раз одновременно. Копии ресурса
разделяют данные; активные голоса удерживают их, даже если уничтожить все
копии `sound`. Уничтожение `voice_handle` также не останавливает голос.
Пустой `sound{}` преобразуется к `false`, попытка запустить его возвращает
`error_code::invalid_argument`.

## Голоса, повторы и громкость

Дальнейшие фрагменты рассчитаны на код внутри функции, возвращающей `void`.
В них `sound` — успешно загруженный объект `sound`, а `voice` — полученный
из `try_play()` handle.

```cpp
auto started = mngl::audio::try_play(sound, {.volume = 0.8f, .loop = true, .bus = mngl::audio::bus::sfx});
if (!started)
{
	std::cerr << started.error().message << '\n';
	return;
}
auto voice = *started;

auto paused = mngl::audio::try_pause(voice);
if (!paused)
	std::cerr << paused.error().message << '\n';

auto resumed = mngl::audio::try_resume(voice);
if (!resumed)
	std::cerr << resumed.error().message << '\n';

auto stopped = mngl::audio::try_stop(voice);
if (!stopped)
	std::cerr << stopped.error().message << '\n';
```

| Настройка | Значение по умолчанию и поведение |
|---|---|
| `play_options::volume` | `1.0f`; конечные значения ограничиваются диапазоном `[0, 1]` |
| `play_options::loop` | `false`; `true` повторяет голос бесконечно |
| `play_options::bus` | `bus::sfx`; можно выбрать `bus::music` |
| `play_options::position` | без позиции; указывает пространственное положение источника |
| `try_play(music)` без options | Использует шину `music` |
| `try_play(music, options)` | Использует переданный `options.bus`; при пропуске этого поля оно остаётся `sfx` |

Громкостью управляют `try_set_master_volume(float)` и
`try_set_bus_volume(bus, float)`. Получить уровни можно через `try_master_volume()`
и `try_bus_volume(bus)`. Изменения применяются к активным и будущим голосам;
громкость каждого голоса умножается на уровень его шины и master.
NaN и бесконечности возвращают `invalid_argument`.

```cpp
auto changed = mngl::audio::try_set_bus_volume(mngl::audio::bus::music, 0.5f);
if (!changed)
	std::cerr << changed.error().message << '\n';
```

`voice_handle` обозначает конкретный запуск. ID не переиспользуются;
старый handle никогда не начнёт управлять новым голосом. После остановки
или естественного завершения `try_is_playing()` и `try_is_paused()` возвращают
успешный результат `false`, а `try_pause()`, `try_resume()` и `try_stop()` — ошибку
`voice_inactive`. У приостановленного голоса `try_is_playing()` равно `false`,
`try_is_paused()` равно `true`. Завершённые tracks очищаются при следующем
аудиовызове или завершении runtime.

Запрос состояния возвращает `std::expected<bool, error>`. Проверка
`if (result)` означает успешность запроса; само состояние находится в
`*result`:

```cpp
auto playing = mngl::audio::try_is_playing(voice);
if (!playing)
{
	std::cerr << playing.error().message << '\n';
}
else if (*playing)
{
	// Этот запуск сейчас воспроизводится.
}
```

## Потоковая музыка из файла

`try_load_music(path)` проверяет источник, не загружая файл целиком в память.
При каждом `try_play()` открывается отдельный поток и данные декодируются
во время воспроизведения. Файл должен быть доступен и при запуске голоса.

```cpp
auto music = mngl::audio::try_load_music("assets/theme.ogg");
if (!music)
{
	std::cerr << music.error().message << '\n';
	return;
}
auto started = mngl::audio::try_play(*music, {.volume = 0.7f, .loop = true, .bus = mngl::audio::bus::music});
if (!started)
{
	std::cerr << started.error().message << '\n';
	return;
}
```

Музыка использует те же команды управления голосами. Копии `music`
разделяют источник; параллельные запуски имеют независимые позиции чтения.
Уничтожение `music` не обрывает уже запущенные голоса.

## Музыка из C++ потока

`try_load_music(std::istream&)` принимает seekable поток, включая `std::iostream`,
`std::ifstream` и `std::fstream`. Источник читается от позиции, зафиксированной
при загрузке, до конца. Поток нужно открыть в бинарном режиме, сохранить
живым и открытым до уничтожения всех копий `music` и остановки/очистки его
голосов. Перед закрытием заимствованного потока остановите его голоса через
`try_stop()`; завершённые tracks также очищаются запросом их состояния.
Во время воспроизведения не читайте, не перематывайте и не закрывайте его
из пользовательского кода.

Для автоматического владения источником используйте перегрузку с
`std::shared_ptr<std::istream>`:

```cpp
// Дополнительно нужны <fstream> и <memory>.
auto source = std::make_shared<std::fstream>("assets/theme.ogg", std::ios::in | std::ios::binary);
auto music = mngl::audio::try_load_music(source);
if (!music)
{
	std::cerr << music.error().message << '\n';
	return;
}
auto started = mngl::audio::try_play(*music);
if (!started)
	std::cerr << started.error().message << '\n';
```

В этой перегрузке ресурс и активные голоса удерживают shared pointer.
Сам SDL3_mixer получает адаптер чтения, а не копию всего C++ потока.
Неперематываемый источник для `music` не подходит.

## Пространственное позиционирование

Передайте `position3d` при запуске или измените положение уже работающего
голоса через `try_set_position()` / `set_position()`. Передайте `std::nullopt`,
чтобы отключить позиционирование:

```cpp
auto started = mngl::audio::try_play(
    sound, {.position = mngl::audio::position3d{4.0f, 0.0f, -2.0f}});
if (!started)
	return;

auto voice = *started;
(void)mngl::audio::try_set_position(voice, mngl::audio::position3d{2.0f, 0.0f, -1.0f});
(void)mngl::audio::try_set_position(voice, std::nullopt);
```

Слушатель закреплён в начале координат. Ось X направлена вправо, Y вверх,
отрицательная Z — вперёд. Микшер сводит позиционируемый источник к моно и
расставляет звук по доступным выходным каналам с базовым затуханием расстояния.
Это удобная игровая панорама, без перемещаемого слушателя, эффекта Доплера и
настраиваемой модели затухания. Для обычной стереомузыки оставляйте `position`
пустым.

## Захват звука

`recording_devices()` перечисляет текущие устройства записи. Без аргумента
`open_capture()` использует системное устройство по умолчанию; вариант с
`recording_device` открывает выбранное устройство. Сессия начинает запись сразу.

Пустой список означает успешное перечисление без доступных устройств.
Если SDL не смог выполнить перечисление, `try_recording_devices()` возвращает
ошибку с кодом `capture_device`, а `recording_devices()` выбрасывает `audio_error`.
`read()` забирает все доступные interleaved float samples, `pause()` и `resume()`
управляют устройством. Размер порции меняется от чтения к чтению, поэтому
проверяйте длину вектора; число кадров равно `samples.size() / channels()`.

```cpp
auto capture = mngl::audio::try_open_capture({.sample_rate = 48000, .channels = 1});
if (!capture)
	return;

auto samples = capture->try_read(); // Может вернуть пустой вектор, если данных пока нет.
```

Для открытия конкретного устройства получите список через `try_recording_devices()`
и передайте `devices->front()` в `try_open_capture()`. Список — снимок: после отключения и повторного подключения устройства
его handle может устареть. `capture_options` задаёт частоту дискретизации и один
или два канала; данные возвращаются как interleaved `float` samples. SDL записывает
данные на своём аудиопотоке, поэтому приложению не нужны callbacks или
покадровая обработка capture.

## Пользовательский живой источник

`audio_stream` принимает interleaved PCM `float` и подаёт его в общий микшер.
Это короткий путь для процедурного аудио, декодера приложения или сетевого
голоса. Стандартный формат — 48 кГц, два канала; поддерживаются один и два
канала. Можно накопить небольшой буфер перед запуском, чтобы снизить риск
провалов, затем дописывать звук по мере появления:

```cpp
#include <vector>

auto source = mngl::audio::create_audio_stream({.sample_rate = 48000, .channels = 1});
std::vector<float> block(480, 0.0f); // 10 мс моносигнала при 48 кГц.
source.write(block);                 // Запись допускается из рабочего потока.

auto voice = mngl::audio::play(source);
// Позже producer может вызвать source.write(next_block).
// Когда источник больше не нужен:
mngl::audio::stop(voice);
```

У `audio_stream` может быть только один активный голос. Объект-голос удерживает
источник живым; удаление handle само по себе не останавливает звук. Источник
остаётся активным, даже если буфер временно опустел, поэтому тишина при
недостаточной буферизации возможна. `loop` для такого источника недопустим:
повторение данных контролирует producer. `write()` потокобезопасен относительно
микширования, но приложение не должно одновременно перемещать или уничтожать
сам объект `audio_stream`. `queued_input_frames()` показывает, сколько исходных
кадров ещё лежит во входной очереди SDL; это ориентир для producer, а не точная
оценка задержки до устройства.

<a id="audio-configuration"></a>

## Отключение и собственные интерфейсы

| Define | Что остаётся доступно |
|---|---|
| Без defines | `mngl::audio` поверх SDL3_mixer |
| `MNGL_CUSTOM_MIXER_INTERFACE` | Заголовки SDL3_mixer доступны напрямую; фасад MNGL и создание микшера MNGL отключены |
| `MNGL_CUSTOM_AUDIO_INTERFACE` | Фасад и интеграция SDL3_mixer отключены; SDL audio и аудиособытия остаются |
| `MNGL_CUSTOM_AUDIO_BACKEND` | Аудиоинтеграция MNGL отключена целиком, включая стандартный `SDL_INIT_AUDIO` и forwarding аудиособытий |

При нескольких defines приоритет такой:
`MNGL_CUSTOM_AUDIO_BACKEND` > `MNGL_CUSTOM_AUDIO_INTERFACE` >
`MNGL_CUSTOM_MIXER_INTERFACE`. Режим полного отключения меняет стандартные
`APP_FLAGS`; если приложение задаёт собственные `APP_FLAGS`, их состав
контролирует приложение.

При сборке MNGL из исходников одноимённые CMake options передаются target:

```sh
cmake -S . -B build -DMNGL_CUSTOM_AUDIO_INTERFACE=ON
```

При использовании готового пакета опция в кеше стороннего проекта не
переконфигурирует импортированный MNGL. Для дополнительного отключения
добавьте define приложению:

```cmake
target_compile_definitions(my_game PRIVATE MNGL_CUSTOM_AUDIO_INTERFACE)
```

Если слой исключён уже при сборке development-пакета, для его возвращения
нужен другой пакет или сборка из исходников. В Visual Studio проект MNGL
принимает одноимённые MSBuild properties со значениями `true`/`false`.
Собственный проект Visual Studio задаёт defines и пути зависимостей сам.
Определения должны совпадать во всех единицах трансляции приложения.

Фасад не содержит регистрации пользовательских backend-объектов: выбранный
define освобождает соответствующий слой для реализации приложения.

## Ошибки и ограничения

`error` содержит `code` и `message`. Загрузка и запуск возвращают
`std::expected<Resource, error>` или `std::expected<voice_handle, error>`;
команды — `std::expected<void, error>`, запросы — expected соответствующего
значения. Ошибки runtime, декодирования, аргументов и неактивного голоса
обрабатывайте в месте вызова.

В аудио v2 пока нет выбора playback-устройства, перемещения слушателя,
настроек модели затухания, seek/fade и пользовательских шин. Контракты новых
возможностей описаны в [проектных решениях аудио v2](../design/audio-v2.md);
история первой версии — в [плане аудио v1](../design/audio-v1.md).
