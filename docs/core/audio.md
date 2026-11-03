# Звуки и потоковая музыка

Заголовок `<MNGL/audio.hpp>` предоставляет `mngl::audio` поверх SDL3_mixer.
`Sound` хранит декодированный эффект в памяти, `Music` читает длинный источник
во время воспроизведения. Каждый вызов `play()` создаёт независимый голос
и возвращает `VoiceHandle`; управление одинаково для обоих типов ресурсов.

<a id="sdl3-mixer"></a>

## Подключение SDL3_mixer

В текущем `main` заголовки SDL3_mixer подключены через submodule
`third_party/SDL_mixer`, закреплённый на `release-3.2.4`. Автоматическая
линковка и включение его бинарников в CI/CD-пакеты ещё не добавлены.
Само подключение `MNGL::MNGL` пока не добавляет SDL3_mixer к линковке.

Если SDL3_mixer установлен с собственным CMake config, добавьте к проекту:

```cmake
find_package(SDL3_mixer CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE SDL3_mixer::SDL3_mixer)
```

Так можно подключить и development-пакет MNGL, и исходники через
`add_subdirectory()`. Префикс SDL3_mixer добавьте в `CMAKE_PREFIX_PATH`.
При ручном подключении в MSVC нужны include path, `SDL3_mixer.lib` и
runtime `SDL3_mixer.dll`; остальные зависимости зависят от сборки микшера.
Используйте совместимые бинарники SDL3 и SDL3_mixer той же архитектуры.
Поддержка форматов определяется декодерами, включёнными в SDL3_mixer.

Без использования `audio.hpp` приложению не нужны символы микшера MNGL.
Полностью отключить соответствующие слои можно defines из таблицы ниже.

## Запуск и время жизни

Все публичные аудиовызовы выполняйте в основном потоке. Создавать `Engine`
вручную не нужно: первый вызов загрузки или воспроизведения лениво создаёт
один микшер на системном устройстве воспроизведения по умолчанию.
Ошибка запуска возвращается в `std::expected`; следующий вызов может
повторить попытку создания.

Аудио работает независимо от `Input::continue_work()`. Функции `audio.update()`
нет. Внутренний `Engine` останавливает голоса и освобождает SDL3_mixer до
`SDL_Quit()` при завершении программы. Не вызывайте `SDL_Quit()` вручную,
пока используете runtime MNGL. Потеря фокуса окна сама по себе не ставит
голоса на паузу — это решение приложения.

## Загрузка и воспроизведение эффекта

`load_sound(path)` принимает UTF-8 путь и предварительно декодирует эффект
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
    if (SDL_Runtime) {
        std::cerr << std::string(SDL_Runtime) << '\n';
        return 1;
    }

    try {
        mngl::window window("Sound", { 800, 600 });
        auto sound = mngl::audio::load_sound("assets/click.wav");
        if (!sound) {
            std::cerr << sound.error().message << '\n';
            return 1;
        }

        while (mngl::Input::continue_work()) {
            for (const auto& keyboard : mngl::Input::keyboards()) {
                if (keyboard.pressed(mngl::scancode::space)) {
                    auto voice = mngl::audio::play(*sound);
                    if (!voice) {
                        std::cerr << voice.error().message << '\n';
                    }
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
```

Один `Sound` можно запускать несколько раз одновременно. Копии ресурса
разделяют данные; активные голоса удерживают их, даже если уничтожить все
копии `Sound`. Уничтожение `VoiceHandle` также не останавливает голос.
Пустой `Sound{}` преобразуется к `false`, попытка запустить его возвращает
`AudioErrorCode::invalid_argument`.

## Голоса, повторы и громкость

Дальнейшие фрагменты рассчитаны на код внутри функции, возвращающей `void`.
В них `sound` — успешно загруженный объект `Sound`, а `voice` — полученный
из `play()` handle.

```cpp
auto started = mngl::audio::play(sound, {
    .volume = 0.8f,
    .loop = true,
    .bus = mngl::audio::Bus::sfx
});
if (!started) {
    std::cerr << started.error().message << '\n';
    return;
}
auto voice = *started;

auto paused = mngl::audio::pause(voice);
if (!paused) std::cerr << paused.error().message << '\n';

auto resumed = mngl::audio::resume(voice);
if (!resumed) std::cerr << resumed.error().message << '\n';

auto stopped = mngl::audio::stop(voice);
if (!stopped) std::cerr << stopped.error().message << '\n';
```

| Настройка | Значение по умолчанию и поведение |
|---|---|
| `PlayOptions::volume` | `1.0f`; конечные значения ограничиваются диапазоном `[0, 1]` |
| `PlayOptions::loop` | `false`; `true` повторяет голос бесконечно |
| `PlayOptions::bus` | `Bus::sfx`; можно выбрать `Bus::music` |
| `play(music)` без options | Использует шину `music` |
| `play(music, options)` | Использует переданный `options.bus`; при пропуске этого поля оно остаётся `sfx` |

Громкостью управляют `set_master_volume(float)` и
`set_bus_volume(Bus, float)`. Получить уровни можно через `master_volume()`
и `bus_volume(Bus)`. Изменения применяются к активным и будущим голосам;
громкость каждого голоса умножается на уровень его шины и master.
NaN и бесконечности возвращают `invalid_argument`.

```cpp
auto changed = mngl::audio::set_bus_volume(mngl::audio::Bus::music, 0.5f);
if (!changed) std::cerr << changed.error().message << '\n';
```

`VoiceHandle` обозначает конкретный запуск. ID не переиспользуются;
старый handle никогда не начнёт управлять новым голосом. После остановки
или естественного завершения `is_playing()` и `is_paused()` возвращают
успешный результат `false`, а `pause()`, `resume()` и `stop()` — ошибку
`voice_inactive`. У приостановленного голоса `is_playing()` равно `false`,
`is_paused()` равно `true`. Завершённые tracks очищаются при следующем
аудиовызове или завершении runtime.

Запрос состояния возвращает `std::expected<bool, AudioError>`. Проверка
`if (result)` означает успешность запроса; само состояние находится в
`*result`:

```cpp
auto playing = mngl::audio::is_playing(voice);
if (!playing) {
    std::cerr << playing.error().message << '\n';
} else if (*playing) {
    // Этот запуск сейчас воспроизводится.
}
```

## Потоковая музыка из файла

`load_music(path)` проверяет источник, не загружая файл целиком в память.
При каждом `play()` открывается отдельный поток и данные декодируются
во время воспроизведения. Файл должен быть доступен и при запуске голоса.

```cpp
auto music = mngl::audio::load_music("assets/theme.ogg");
if (!music) {
    std::cerr << music.error().message << '\n';
    return;
}
auto started = mngl::audio::play(*music, {
    .volume = 0.7f,
    .loop = true,
    .bus = mngl::audio::Bus::music
});
if (!started) {
    std::cerr << started.error().message << '\n';
    return;
}
```

Музыка использует те же команды управления голосами. Копии `Music`
разделяют источник; параллельные запуски имеют независимые позиции чтения.
Уничтожение `Music` не обрывает уже запущенные голоса.

## Музыка из C++ потока

`load_music(std::istream&)` принимает seekable поток, включая `std::iostream`,
`std::ifstream` и `std::fstream`. Источник читается от позиции, зафиксированной
при загрузке, до конца. Поток нужно открыть в бинарном режиме, сохранить
живым и открытым до уничтожения всех копий `Music` и остановки/очистки его
голосов. Перед закрытием заимствованного потока остановите его голоса через
`stop()`; завершённые tracks также очищаются запросом их состояния.
Во время воспроизведения не читайте, не перематывайте и не закрывайте его
из пользовательского кода.

Для автоматического владения источником используйте перегрузку с
`std::shared_ptr<std::istream>`:

```cpp
// Дополнительно нужны <fstream> и <memory>.
auto source = std::make_shared<std::fstream>(
    "assets/theme.ogg", std::ios::in | std::ios::binary);
auto music = mngl::audio::load_music(source);
if (!music) {
    std::cerr << music.error().message << '\n';
    return;
}
auto started = mngl::audio::play(*music);
if (!started) std::cerr << started.error().message << '\n';
```

В этой перегрузке ресурс и активные голоса удерживают shared pointer.
Сам SDL3_mixer получает адаптер чтения, а не копию всего C++ потока.
Неперематываемый источник для `Music` не подходит.

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

`AudioError` содержит `code` и `message`. Загрузка и запуск возвращают
`std::expected<Resource, AudioError>` или `std::expected<VoiceHandle, AudioError>`;
команды — `std::expected<void, AudioError>`, запросы — expected соответствующего
значения. Ошибки runtime, декодирования, аргументов и неактивного голоса
обрабатывайте в месте вызова.

В первой версии нет публичного выбора аудиоустройства, записи, seek/fade,
пользовательских шин и пространственного аудио. Решения о границах API и
дальнейшие этапы собраны в [плане аудио v1](../design/audio-v1.md).
