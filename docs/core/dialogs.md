# Системные диалоги

`<MNGL/dialog.hpp>` открывает системные диалоги SDL для выбора файла,
нескольких файлов, места сохранения и папки. Вызов сразу возвращает
`mngl::dialog::request`; результат появляется позже и проверяется через
`request::result()`.

## Открыть один файл

```cpp
#include <MNGL/dialog.hpp>
#include <MNGL/input.hpp>
#include <MNGL/window.hpp>
#include <utility>

mngl::window window("Open a file", {800, 600});

mngl::dialog::file_dialog_options options;
options.parent = &window;
options.filters = {{"Images", "png;jpg;jpeg"}, {"All files", "*"}};

auto request = mngl::dialog::open_file(std::move(options));
bool finished = false;
while (!finished && mngl::input::next_frame())
{
	const auto result = request.result();
	switch (result.status)
	{
	case mngl::dialog::request_status::pending:
		break;
	case mngl::dialog::request_status::selected:
		// result.paths содержит выбранный std::filesystem::path.
		finished = true;
		break;
	case mngl::dialog::request_status::cancelled:
		finished = true;
		break;
	case mngl::dialog::request_status::failed:
		logger.error("File dialog failed: ", result.error);
		finished = true;
		break;
	}
}
```

Если параметры по умолчанию подходят, вызовите `open_file()` без аргументов.
`file_dialog_options` позволяет задать родительское окно, начальный файл или
каталог и список фильтров. SDL принимает шаблоны фильтров как расширения,
разделённые `;`, например `"png;jpg"`; `"*"` означает все файлы.
Поддержка фильтров и начального пути зависит от платформы, система может их
игнорировать.

## Остальные диалоги

`open_files(options)` разрешает выбрать несколько файлов и возвращает каждый
путь в `result.paths`. `save_file(options)` предлагает путь нового или
существующего файла; само сохранение остаётся за приложением. Для выбора
папки используйте `open_folder(folder_options)`. Папки не принимают фильтры.

Начальный путь задаётся как `std::filesystem::path`:

```cpp
mngl::dialog::file_dialog_options open_options;
open_options.parent = &window;
open_options.default_location = "levels";
auto files = mngl::dialog::open_files(std::move(open_options));

mngl::dialog::folder_dialog_options folder_options;
folder_options.parent = &window;
auto folder = mngl::dialog::open_folder(std::move(folder_options));
```

`result.status` принимает `pending`, `selected`, `cancelled` или `failed`.
Пути и индекс выбранного фильтра доступны после `selected`; `selected_filter`
может быть пустым, если платформа не сообщает фильтр. При `failed` текст
ошибки находится в `result.error`. Отмена не считается ошибкой.

## Потоки и время жизни

SDL запускает диалог асинхронно; callback может прийти с другого потока.
MNGL копирует результат в защищённое состояние, поэтому `request::result()`
можно вызывать из игрового цикла. Фильтры и начальный путь копируются в
контекст запроса и остаются живыми до callback.

`request::ready()` проверяет только статус без копирования выбранных путей.
Нулевой символ внутри начального пути, имени или шаблона фильтра приводит к
`std::invalid_argument` до запуска диалога. Количество фильтров должно помещаться
в SDL `int`, иначе выбрасывается `std::length_error`.

Создавайте системный диалог из главного потока. Если задано `options.parent`,
родительское `mngl::window` должно оставаться живым до завершения запроса.
Удаление `request` не закрывает уже показанный системный диалог; оно лишь
отбрасывает возможность прочитать результат. Когда результат больше не
нужен, callback сам освобождает внутреннее состояние.

В Linux системный диалог может использовать XDG Portal и требует работающий
цикл событий SDL. `input::next_frame()` обрабатывает события MNGL; не
запускайте дополнительный цикл `SDL_PollEvent()`. Само открытие диалога и
чтение готового результата не требуют отдельного `update()`.

SDL на Android может вернуть `content://` URI. На настольных системах
результаты являются обычными путями файловой системы; для Android URI нужно
открывать через SDL IO API, а не `std::ifstream`.
