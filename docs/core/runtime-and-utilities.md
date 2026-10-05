# Runtime и утилиты

## Инициализация SDL

MNGL автоматически инициализирует SDL через глобальный `SDL_Runtime` и завершает его при завершении программы. По умолчанию включены подсистемы SDL для audio, video, gamepad, haptic, sensor и camera. `APP_NAME`, `APP_VERSION`, `APP_ID` и `APP_FLAGS` можно переопределить на уровне сборки до подключения заголовков MNGL.

Для ошибок SDL используется глобальный `sdl_error`, наследующийся от `std::runtime_error` и добавляющий текст последней ошибки SDL. Внутренние проверки библиотеки используют макросы `TRY` и `TRY_SDL`.

## Логирование

Глобальный `logger` (тип `Logger`) пишет сообщения в консоль и `last.log`. Доступны уровни `log`, `debug`, `info`, `success`, `warning`, `error` и `critical`; `debug` по умолчанию включён в Debug-сборке. `enableDebug()`, `disableDebug()` и `setDebug(bool)` меняют его во время работы, а `flush()` сбрасывает буферы вывода.

```cpp
logger.info("Loaded texture: ", image.width(), " x ", image.height());
logger.error("Failed to load scene");
```

## Обновляемые объекты и interop

Наследник `mngl::Iupdatable` регистрируется при создании и удаляется из `update_manager` при уничтожении. `Input::continue_work()` вызывает его `update(double dt)` каждый кадр. Этот механизм рассчитан на простые глобальные обновления, например анимацию, а не на сложные иерархии объектов.

`mngl::Proxy` и alias вроде `rectangleProxy` — низкоуровневые адаптеры для передачи типов MNGL в функции SDL, принимающие указатели на SDL-структуры. Для обычного прикладного кода обращаться к ним напрямую не требуется. `read_binary_file(path)` — утилита чтения файла в `std::vector<uint8_t>`.
