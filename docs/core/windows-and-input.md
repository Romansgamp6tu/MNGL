# Окна и ввод

## Окно и дисплеи

`mngl::window` — RAII-обёртка окна SDL. Конструктор принимает заголовок, размер и необязательные флаги:

```cpp
mngl::window::flags flags{};
flags.resizeable = true;

mngl::window window("My app", { 1280, 720 }, flags);
```

Флаги окна задаются полями `window::flags`; среди них fullscreen, borderless, high-pixel-density, modal и platform graphics backend. Большинство свойств окна читаются и меняются через пары методов вроде `size()` / `size(new_size)` и `title()` / `title(new_title)`. Для 2D-отрисовки используйте `mngl::render_window`, который расширяет окно рендерингом.

`window::size()` возвращает размер окна в оконных координатах, а `pixel_size()` — фактический размер drawable-области в пикселях. Они могут различаться на дисплеях с высокой плотностью пикселей.

`mngl::display` позволяет получить список дисплеев (`displays()`), основной дисплей (`primary()`), границы, рабочую область, масштаб и видеорежимы. Окно предоставляет удобные методы `attached_display()` и `mode()`.

## Основной цикл и состояние ввода

Обычно приложение вызывает `Input::continue_work()` один раз за кадр:

```cpp
#include "input.hpp"

while (mngl::Input::continue_work())
{
    if (mngl::Input::is_key_pressed(mngl::scancode::a))
    {
        // Обработать нажатие в текущем кадре.
    }

    const double dt = mngl::Input::get_dt();
    // Обновить состояние приложения и отрисовать кадр.
}
```

`continue_work()` обрабатывает события, обновляет состояние ввода и вызывает `update(dt)` у зарегистрированных `mngl::Iupdatable`. Он возвращает `false` после запроса закрытия приложения или когда `Input::should_work` сброшен. `get_dt()` возвращает время между кадрами в секундах.

Для клавиатуры доступны `is_key_pressed`, `is_key_holded` и `is_key_released`; код клавиши задаётся `mngl::scancode`. Аналогичные методы есть для кнопок мыши. Положение указателя, его смещение и прокрутка доступны через `get_mouse_position()`, `get_mouse_delta()` и `get_mouse_scroll_delta()`.

Ограничение частоты кадров по умолчанию равно 60 FPS. Вызов `set_max_fps(0)` отключает ожидание между кадрами.

## Dear ImGui

`mngl::imgui_context` подключает Dear ImGui к `mngl::render_window` через SDL3 и SDL_GPU. UI рисуется отдельным GPU-проходом поверх swapchain-текстуры — не через `Frame2D` и не как набор 2D-спрайтов.

```cpp
#include "imgui_compat.hpp"
#include "input.hpp"
#include "render_window.hpp"

#include <format>
#include <regex>
#include <string>
#include <vector>

mngl::render_window window("My app", { 1280, 720 });
mngl::imgui_context gui(window);
mngl::texture preview = window.create_texture({ 400, 300 });
preview.clear();
preview.display();
int clicks = 0;
std::string username;
const std::regex username_pattern(R"([A-Za-z0-9_]*)");

while (mngl::Input::continue_work())
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
            panel.image(preview, { 180.0f, 135.0f });
        }
    } // Завершает ImGui-кадр до window.display().

    window.clear();
    window.display();
}
```

Создавайте ImGui-кадр после `Input::continue_work()` и завершайте его до `render_window::display()`: `display()` добавляет UI-проход после 2D-кадра. `Input` передаёт SDL-события backend-у на главном потоке и не отдаёт игре клавиатурный или мышиный ввод, захваченный интерфейсом. Обёртки `frame_scope`, `window_scope`, `dockspace_scope`, `main_menu_bar_scope` и `menu_scope` управляют временем жизни кадра, окон, меню и dockable-панелей; доступны `window()`, `dockspace()`, `main_menu_bar()`, `menu()`, `toggle()`, `text()`, `button()`, `input()`, `checkbox()`, `selectable()`, `context_menu()`, `radio_button()`, `bullet()`, `progress_bar()`, `slider()`, `combo()` и `image()`.

Чтобы выводить весь ImGui workspace в одну offscreen-текстуру, передайте её в `gui.frame(surface)`. Текстура должна быть доступна для рендеринга и сэмплирования, использовать формат swapchain и оставаться живой до `window.display()`. После завершения ImGui scope отрисуйте её как обычный `mngl::sprite`:

```cpp
auto surface = window.create_texture(window.size());
const auto viewport = window.coordinate_size();
mngl::sprite workspace_sprite(
    surface,
    mngl::rectangle{ 0.0f, 0.0f, viewport.x, viewport.y });
mngl::imgui_context gui(window);
window.enable_hit_testing();

while (mngl::Input::continue_work())
{
    {
        auto frame = gui.frame(surface);
        auto host = frame.window("Workspace");
        auto docks = host.dockspace("MainWorkspace");
        auto panel = docks.window("Controls");
        if (panel)
        {
            panel.text("The complete workspace is one texture.");
        }
    }

    window.clear(surface, mngl::color{ 0.0f, 0.0f, 0.0f, 0.0f });
    window.display();

    window.clear();
    window.draw(workspace_sprite);
    window.display();
}
```

Для маршрутизации мыши включите hit-testing на `render_window` до первого кадра (`window.enable_hit_testing()`). `gui.frame(surface)` находит поверхность в hit-test-пути верхнего спрайта и преобразует курсор в пиксели текстуры, учитывая размер, поворот и UV-область спрайта. Наведение вне поверхности по-прежнему передаётся сцене, но начатое на ней перетаскивание продолжает получать координаты за её границами; продолжение за границами OS-окна зависит от поддержки захвата мыши SDL платформой. Клавиатурные и текстовые события по-прежнему приходят через SDL backend.

Демо `MNGL.exe` служит ручным стендом: окно можно менять по размеру, режимы в панели Controls проверяют перенос, масштаб, поворот и UV crop поверхности, а `F1` скрывает её, чтобы проверить hit-test кнопки сцены под ней. Прозрачные участки спрайта пока считаются попаданием; начав перетаскивание на поверхности, его можно продолжить за её границами.

Docking включён в vendored-версии Dear ImGui. `window_scope::dockspace(id)` создаёт область внутри текущего окна, а `dockspace_scope::window(title, open)` привязывает панель к ней при первом открытии. Панели стартуют вкладками в общей области; перетаскивайте вкладку к краю, чтобы разделить её, или выбирайте вкладку, чтобы переключиться. Панели можно отсоединить и свободно перемещать внутри поверхности ImGui; multi-viewport не включён, поэтому отдельные OS-окна не создаются. Закрытые панели можно вернуть из пункта `View`: `menu_scope::toggle(label, bool&)` синхронизирует пункт меню с состоянием панели.

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
glm::vec2 position{ 0.2f, 0.5f };
glm::ivec2 grid_position{ 2, 4 };

panel.slider("Position", position, -1.0f, 1.0f);
panel.slider("Grid position", grid_position, -20, 20);
```

`combo(label, values)` строит выпадающий список из конечного forward-range. Элементы должны преобразовываться в `std::string_view`/строку или иметь доступный через ADL либо `std::to_string` вызов `to_string(value)`. Для собственного типа объявите `to_string(const T&)` в его ассоциированном namespace. Результат — `std::optional`, возвращаемый каждый кадр: `nullopt` означает, что выбор отсутствует, а пункт `(none)` позволяет его сбросить. Выбор сохраняется внутри ImGui по ID виджета, поэтому соблюдайте уникальность подписей (или используйте `##id`, как в ImGui). Для lvalue-контейнера или borrowed-range возвращается итератор (указатель для массива); для временного контейнера — копия элемента, если она копируема, иначе индекс. Итератор используйте, пока контейнер не изменён; не сохраняйте его через операции, которые могут инвалидировать итераторы.

```cpp
const std::vector<std::string> modes{ "Solid", "Wireframe", "Points" };
if (const auto selected = panel.combo("Render mode", modes))
{
    panel.text(**selected); // optional<iterator>
}

if (const auto quality = panel.combo(
    "Quality",
    std::initializer_list<std::string>{ "Low", "Medium", "High" }))
{
    panel.text(*quality); // optional<string> для временного списка
}
```

`context_menu(values)` открывает контекстное меню по правому клику на последнем ImGui-элементе. Вызывайте его сразу после виджета с собственным ID, например `selectable()` или `button()`. Список принимает те же диапазоны и типы подписей, что и `combo()`, но результат появляется только в кадре, когда пользователь выбрал пункт; в остальных случаях возвращается пустой `std::optional`. Для lvalue-контейнера результат содержит итератор, для временного — копию элемента, если она доступна, иначе индекс. Выбор не сохраняется между кадрами: это удобно для команд меню.

```cpp
const std::vector<std::string> actions{ "Rename", "Delete" };

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

По умолчанию используется встроенный масштабируемый векторный шрифт ImGui размером 18 px. Чтобы загрузить TTF/OTF с поддержкой кириллицы и задать остальные начертания, передайте пути в конфигурации:

```cpp
mngl::imgui_compat::font_config fonts;
fonts.regular.file = "assets/fonts/Regular.ttf";
fonts.bold.file = "assets/fonts/Bold.ttf";
fonts.monospace.file = "assets/fonts/Mono.ttf";
fonts.regular.size_pixels = 18.0f;

mngl::render_window window("My app", { 1280, 720 });
mngl::imgui_context gui(window, fonts);

while (mngl::Input::continue_work())
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

Пути шрифтов трактуются относительно текущей рабочей папки процесса; пустой путь у начертания использует regular-шрифт. Размеры задаются в логических пикселях и могут меняться через `use_font()`. SDL3 backend учитывает framebuffer scale окна, поэтому не нужно вручную увеличивать шрифт только из-за high-DPI дисплея. Контекст должен жить не дольше своего `render_window`; поддерживается один активный контекст, и кадры создаются на SDL main thread.
