# Буфер обмена

`<MNGL/clipboard.hpp>` предоставляет системный буфер обмена SDL3. Для обычного
текста используйте UTF-8-функции; для изображений и других типов данных — API
по MIME-типам.

Примеры в этой главе — фрагменты функций приложения. SDL clipboard API
вызывайте из главного потока после запуска runtime; готовое приложение может
проверить запуск через [`runtime::ensure()`](runtime-and-utilities.md).

## Текст

```cpp
#include <MNGL/clipboard.hpp>

mngl::clipboard::set_text("Copied from MNGL");

if (mngl::clipboard::has_text())
{
	const std::string text = mngl::clipboard::get_text();
}
```

`set_text()` заменяет текущее содержимое буфера обмена. `get_text()` возвращает
UTF-8-строку; память, выделенную SDL, MNGL освобождает автоматически.

## Данные произвольного типа

Приложение может предложить несколько форматов. Данные генерируются только
когда другая программа запрашивает конкретный MIME-тип:

```cpp
#include <MNGL/clipboard.hpp>

const std::vector<std::byte> serialized_level = {std::byte{0x4D}, std::byte{0x4E}, std::byte{0x47},
                                                 std::byte{0x4C}};

mngl::clipboard::set_data({"application/x-mngl-level", "text/plain"},
                          [serialized_level](std::string_view mime_type)
                              -> std::optional<std::vector<std::byte>>
                          {
	                          if (mime_type == "application/x-mngl-level")
	                          {
		                          return serialized_level;
	                          }
	                          if (mime_type == "text/plain")
	                          {
		                          constexpr std::string_view description = "MNGL level";
		                          std::vector<std::byte> text;
		                          text.reserve(description.size());
		                          for (const unsigned char character : description)
		                          {
			                          text.push_back(static_cast<std::byte>(character));
		                          }
		                          return text;
	                          }
	                          return std::nullopt;
                          });
```

`set_data()` копирует список MIME-типов и сохраняет callback вместе с буфером
обмена. Callback может быть вызван позже, поэтому захватывайте нужные данные по
значению или обеспечьте им нужное время жизни. Вызовы callback сериализуются.
Возвращённые байты остаются живыми, пока SDL не очистит предложение. Верните
`std::nullopt`, если формат сейчас недоступен; исключение из callback также
безопасно перехватывается и трактуется как пустые данные.

Чтение форматов выглядит так:

```cpp
#include <MNGL/logger.hpp>

for (const auto& mime_type : mngl::clipboard::mime_types())
{
	mngl::log.info("Clipboard format: ", mime_type);
}

if (mngl::clipboard::has_data("application/x-mngl-level"))
{
	const auto bytes = mngl::clipboard::get_data("application/x-mngl-level");
}
```

`get_data()` возвращает копию байтов. `clear()` очищает буфер обмена. Ошибки
SDL API выбрасываются как `mngl::sdl_error`; пустой или повторяющийся MIME-тип
вызывает `std::invalid_argument`.

## Primary selection

На X11 и Wayland есть отдельный primary selection: обычно это текст, который
пользователь выделил мышью, не нажимая Copy. MNGL предоставляет его отдельно
от обычного буфера:

```cpp
mngl::clipboard::primary_selection::set_text("Selected text");

if (mngl::clipboard::primary_selection::has_text())
{
	const auto selected = mngl::clipboard::primary_selection::get_text();
}
```

На платформах без primary selection SDL сохраняет установленную строку только
для локального чтения; другие приложения не увидят её.

## Событие изменения

Событие `mngl::clipboard_update` приходит при изменении системного буфера.
Оно содержит признак `owner` и копию списка MIME-типов из события SDL, поэтому
список остаётся доступен после вызова callback:

```cpp
auto clipboard_subscription = mngl::event_manager::on<mngl::clipboard_update>(
    [](const mngl::clipboard_update& event)
    {
	    if (!event.owner)
	    {
		    // Содержимое изменилось извне; при необходимости перечитайте его.
	    }
    });
```

Подписку нужно хранить, пока callback нужен. Изменение собственного содержимого
тоже может породить событие; `owner` позволяет отличать такие обновления.

## Потоки и совместимость

Вызовы API нужно делать из главного потока, как требует SDL. `set_data()` не
копирует содержимое заранее: callback вызывается при запросе формата системой.
Поставщик должен быть готов к отложенному вызову и не должен ссылаться на
временные данные.

Старые методы `mngl::window::clipboard_text()` оставлены для миграции и
помечены `[[deprecated]]`. Используйте свободные функции из
`mngl::clipboard`.

