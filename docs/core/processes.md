# Процессы

`<MNGL/process.hpp>` предоставляет `mngl::process::child_process` для запуска
внешних программ. Передавайте исполняемый файл и каждый аргумент отдельно:
MNGL не запускает shell и не интерпретирует shell-операторы вроде `|`, `&&`
или подстановки переменных.

Для простых команд есть блокирующий `child_process::run()`. Он возвращает
объединённый вывод stdout и stderr вместе с кодом завершения. Следующий
фрагмент можно поместить внутрь `main()`:

```cpp
#include <MNGL/process.hpp>
#include <MNGL/logger.hpp>

#include <iostream>
#include <string_view>

const auto result = mngl::process::child_process::run({"git", "--version"});
std::cout.write(result.output.data(), result.output.size());
if (result.exit_code != 0)
{
	// Команда завершилась с ошибкой.
}
```

Второй аргумент `run()` — необязательная рабочая папка. Вывод хранится как
байты в `std::string`; он может содержать нулевые байты и не обязан быть UTF-8.

## Долгоживущий процесс

`child_process::launch()` возвращает handle запущенного процесса. Ниже фрагмент
из функции приложения; `helper` остаётся владельцем процесса, пока вы читаете
потоки и проверяете завершение. Маршруты стандартных потоков настраиваются
отдельно: `inherited` использует потоки родителя,
`discard` отбрасывает данные, а `pipe` позволяет приложению читать или писать
через методы handle. По умолчанию stdin отбрасывается, а stdout и stderr
перенаправлены в отдельные pipe. Если установить `merge_standard_error = true`,
stderr объединяется со stdout и `standard_error` должен остаться `inherited`.

```cpp
mngl::process::launch_options options;
options.standard_input = mngl::process::stdio_route::pipe;
options.standard_output = mngl::process::stdio_route::pipe;
options.standard_error = mngl::process::stdio_route::pipe;

auto helper = mngl::process::launch({"my-helper", "--watch", "project"}, options);

const std::string_view request = "status\n";
const auto sent = helper.write_stdin(request);
// Если записана только часть запроса, отправьте остаток позже.

const auto output = helper.read_stdout();
const auto errors = helper.read_stderr();
if (output.state == mngl::process::stream_state::ready)
{
	// Обработайте output.data; это может быть неполный фрагмент строки.
}
if (errors.state == mngl::process::stream_state::failed)
{
	mngl::log.error(errors.error);
}

if (const auto exit_code = helper.poll())
{
	// Процесс завершился; exit_code содержит его код.
}
```

Чтение из pipe неблокирующее. `stream_read_result::data` содержит доступный
фрагмент, а `state` принимает значения `ready`, `not_ready`, `end_of_stream` или
`failed`. Данные stdout и stderr нужно регулярно забирать отдельно. Запись в
stdin может быть частичной; `write_stdin()` возвращает фактическое число байт,
поэтому приложение должно повторить запись оставшейся части.

Когда весь запрос отправлен, вызовите `helper.close_stdin()`, чтобы дочерний
процесс получил EOF. Это необходимо для фильтров, которые читают до конца ввода:
без EOF они могут продолжать ждать данные, а `wait()` не вернётся. Повторное
закрытие безопасно; последующий `write_stdin()` выбрасывает `std::logic_error`.
Если закрытие pipe завершилось ошибкой SDL, `close_stdin()` выбрасывает `mngl::sdl_error`.

## Управление временем жизни

- `poll()` возвращает `std::nullopt`, пока процесс работает, и код завершения
  после его окончания.
- `wait()` блокирует вызывающий поток и возвращает код завершения. Сначала
  дочитайте stdout и stderr: если заполнится любой из перенаправленных pipe,
  дочерний процесс может ждать освобождения места, а `wait()` — не вернуться.
- `terminate(force)` останавливает процесс. По умолчанию `force == true`.
- `id()` возвращает идентификатор процесса, если SDL предоставляет его на
  текущей платформе.
- Деструктор `child_process` освобождает handle SDL, но не завершает сам процесс.
  Если процесс нужно остановить, вызовите `terminate()` до уничтожения handle.

`launch()` выбрасывает `std::invalid_argument` для пустого списка аргументов,
нулевых символов внутри аргументов и несовместимых настроек потоков. Ошибки SDL
выбрасываются как `mngl::sdl_error`; вызовы чтения или записи потока, который не
был настроен как `pipe`, выбрасывают `std::logic_error`.

## Связанные главы

Если процесс запускается как часть командного интерфейса приложения, связывайте
его с командами через [реестр консольных команд](console.md). Для файловых
путей из системного диалога смотрите [руководство по диалогам](dialogs.md).
Обычный `run()` блокирует поток; для интерактивного окна используйте
`launch()` и проверяйте готовность через `poll()` внутри своего цикла кадров.

