# Хранилище и файлы

`<MNGL/storage.hpp>` оборачивает SDL3 store в move-only RAII-типы:
`mngl::storage::title_storage` для данных приложения, `user_storage` для
пользовательских файлов и `file_storage` для локальных каталогов разработки.
Каждый объект закрывает SDL storage при уничтожении.

## Данные приложения и сохранения

```cpp
#include <MNGL/storage.hpp>

mngl::storage::title_storage title;
mngl::storage::user_storage user;

if (title.ready())
{
	const auto welcome = title.read_text_file("data/welcome.txt");
}

if (user.ready())
{
	user.create_directory("saves/levels");
	user.write_text_file("saves/levels/slot1.json", "{\"level\": 3}");

	for (const auto& item : user.enumerate("saves/levels"))
	{
		if (item.type == mngl::storage::entry_type::file)
		{
			logger.info(item.relative_path, " (", item.size, " bytes)");
		}
	}
}
```

`title_storage` по умолчанию открывает каталог приложения, который SDL
определяет для платформы. Он предназначен для чтения поставляемых с игрой
ресурсов. `user_storage` использует `APP_ORGANIZATION` и `APP_STORAGE_NAME`;
их можно переопределить аргументами конструктора. Хранилище пользователя
может подключаться асинхронно, поэтому проверяйте `ready()` в игровом цикле,
а не ждите готовности в tight loop. Закрытие `user_storage` сообщает backend,
что текущая операция сохранения завершена, и позволяет ему синхронизировать
данные.

Путь override можно передать в `title_storage(path)`, если каталогу данных
приложения нужен явный корень:

```cpp
mngl::storage::title_storage title("assets");
```

## Файлы и каталоги

- `read_file(path)` читает файл в `std::vector<std::byte>`; `read_text_file`
  возвращает содержимое как `std::string`.
- `write_file(path, bytes)` создаёт или перезаписывает файл; `write_text_file`
  принимает `std::string_view`.
- `create_directory(path)` создаёт каталог и отсутствующие родительские
  каталоги.
- `enumerate(directory)` перечисляет непосредственные записи каталога.
  `entry` содержит имя, относительный путь, тип и размер.
- `close()` закрывает хранилище явно и выбрасывает `mngl::sdl_error`, если
  backend сообщил об ошибке закрытия. Деструктор закрывает его без исключений.

Все пути внутри store должны быть относительными, использовать `/` и не
содержать компонентов `.` или `..`. Пустой путь разрешён для перечисления
корня. API чтения и записи SDL синхронный; для больших файлов не вызывайте его
в чувствительном к задержкам кадре.

Ошибки SDL приводят к `mngl::sdl_error`. Операции чтения и записи до готовности
хранилища выбрасывают `std::logic_error`; проверяйте `ready()` перед доступом.
`title_storage` предназначен для чтения, а `user_storage` — для чтения и записи.
Закрывайте хранилища до завершения SDL; обычно достаточно держать их локальными
объектами приложения, чтобы деструкторы отработали до `SDL_Quit()`.

## Локальное файловое хранилище

`file_storage(root)` открывает указанный локальный каталог и подходит для
инструментов, редакторов и разработки. Для переносимых данных приложения
используйте `title_storage` и `user_storage`, чтобы платформа могла выбрать
подходящий backend.
