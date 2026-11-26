# SVG

`mngl::two_d::svg_object` загружает SVG на CPU, разбирает контуры и готовит треугольную геометрию для существующего 2D renderer. Основное представление остаётся векторной геометрией: при изменении масштаба MNGL не создаёт bitmap-текстуру нового размера.

## Загрузка и отрисовка

```cpp
auto logo = mngl::two_d::svg_object::from_file("assets/logo.svg");

auto frame = window.frame();
frame.draw(logo);
frame.display();
```

Для строкового источника передайте SVG в конструктор или `load()`:

```cpp
mngl::two_d::svg_object logo{svg_source};
```

Загрузка синхронная. XML-разбор, аппроксимация кривых и тесселяция заливок выполняются на CPU. Корневые `width` и `height` задают intrinsic viewport объекта; корневой `viewBox` и `preserveAspectRatio` отображают в него всю SVG-геометрию. `size()` возвращает размер viewport, а `view_box()` — исходные координаты и размеры `viewBox`. Процентные корневые размеры разрешаются относительно `target_size` из `view_tessellation`, если он задан; без такого контекста используется безопасный viewport 300 × 150. Перегрузка `frame.draw()` принимает объект как обычный `mngl::drawable`.

Точность кривых и дуг настраивается при загрузке через `svg_load_options`:

```cpp
mngl::two_d::svg_load_options options;
options.curve_tolerance = 0.05f; // Допустимое отклонение в координатах SVG.
options.max_curve_segments = 250'000; // Лимит кривых сегментов на один SVG-элемент.
auto logo = mngl::two_d::svg_object::from_file("assets/logo.svg", options);
```

По умолчанию `curve_tolerance` задаётся в координатах SVG и равен `0.25`. Лимит `max_curve_segments` по умолчанию равен 1 000 000 на `<path>`, `<circle>`, `<ellipse>` или скруглённый `<rect>`; для `<path>` учитываются все его кривые и дуги вместе.

Чтобы зафиксировать точность с учётом текущего view, передайте view и размер цели в пикселях при загрузке:

```cpp
mngl::two_d::svg_load_options options;
options.view_tessellation = mngl::two_d::svg_view_tessellation{
    camera, window.pixel_size(), 0.35f}; // Максимальное отклонение в экранных пикселях.
auto logo = mngl::two_d::svg_object::from_file("assets/logo.svg", options);
```

Указывайте физический размер той цели, для которой задан view: `window.pixel_size()` для окна или `target.size()` для render texture. Загрузчик учитывает плотность пикселей в видимой области view, трансформации SVG-групп и элементов, а также `svg_object::transform()` на момент загрузки. Допуск фиксируется в полученной геометрии. Если view или размер render target изменились, синхронизируйте геометрию явно:

```cpp
logo.sync_view(camera, window.pixel_size(), 0.35f);
logo.upload(window.gpu_device()); // Повторно загрузить GPU-геометрию после синхронизации.
```

`sync_view()` заново разбирает исходный SVG и пересобирает CPU-геометрию. Вызывайте его после изменений view, размера цели или `svg_object::transform()`, если нужно сохранить тот же экранный допуск. После синхронизации вызовите `upload()` снова; уже созданные `svg_sprite` сохраняют прежнюю геометрию, поэтому их нужно пересоздать. Масштаб `svg_sprite::size()` задаётся отдельно и не известен исходному объекту: при увеличении спрайта пропорционально уменьшите `curve_tolerance_pixels` в настройках или `sync_view()`. Уменьшение пиксельного допуска повышает точность и увеличивает CPU-геометрию и объём GPU-загрузки.

Для GPU mesh достаточно передать устройству загруженный объект:

```cpp
logo.upload(window.gpu_device());
```

`upload(device)` сам создаёт и отправляет командный буфер. Он передаёт GPU подготовленные вершины, не пиксели. Перед загрузкой последовательные части с одинаковым итоговым цветом объединяются в один mesh, а их SVG transform запекаются в вершины. Это уменьшает число GPU-буферов и draw calls, сохраняя порядок частей. Без вызова `upload()` геометрия остаётся на CPU и передаётся renderer при каждом рисовании. `release_gpu()` освобождает постоянные mesh и оставляет CPU-геометрию доступной.

Если загрузку нужно объединить с другими GPU-командами, передайте свой командный буфер и отправьте его после всех операций:

```cpp
mngl::gpu::command_buffer commands(window.gpu_device());
logo.upload(window.gpu_device(), commands);
commands.submit();
```

Перегрузка с явным командным буфером становится GPU-resident только после успешного `submit()`.
Если буфер отменён или уничтожен активным, первая загрузка оставляет доступным CPU-вариант, а
повторная сохраняет предыдущую GPU-версию. Создать `svg_sprite` до завершения первой загрузки нельзя;
при повторной загрузке спрайты продолжают использовать прежние ресурсы до отправки нового буфера.

`transform(value)` меняет положение и масштаб всего объекта, не пересоздавая геометрию. `tint(color)` умножает исходные цвета SVG на общий tint, а `blend(mode)` задаёт режим смешивания для всех частей.

## GPU-экземпляры

Для одного GPU-спрайта можно сразу загрузить файл на устройство:

```cpp
auto logo = mngl::two_d::svg_sprite::from_file(window.gpu_device(), "assets/logo.svg");

auto frame = window.frame();
frame.draw(logo);
frame.display();
```

`svg_sprite` хранит собственные положение, размер, pivot и поворот. Для нескольких экземпляров загрузите геометрию один раз и создайте спрайты от общего объекта. Они разделяют постоянные GPU-буферы; настройки каждого экземпляра независимы:

```cpp
auto logo = mngl::two_d::svg_object::from_file("assets/logo.svg");
logo.upload(window.gpu_device());

mngl::two_d::svg_sprite large{logo};
mngl::two_d::svg_sprite small{logo};
large.position({0.0f, 0.0f}).size({1.0f, 1.0f});
small.position({1.2f, 0.0f}).size({0.5f, 0.5f}).rotation(0.2f);

auto frame = window.frame();
frame.draw(large);
frame.draw(small);
frame.display();
```

`svg_sprite` владеет общими ссылками на GPU mesh, поэтому исходный `svg_object` можно освободить после создания экземпляров. Масштабирование меняет матрицы экземпляров и не загружает вершины повторно.

## Попадание по SVG-геометрии

Сохранение областей для проверки попадания включается при загрузке и выключено по умолчанию. После этого `window.hit_test()` и `on_click()` возвращают конкретную фигуру по её заливке или обводке:

```cpp
mngl::two_d::svg_load_options options;
options.hit_testable = true;
auto icon = mngl::two_d::svg_sprite::from_file(
    window.gpu_device(), "assets/icon.svg", options);

icon.on_click([](const mngl::hit_test_result&, const mngl::hit_test_path_entry& clicked)
              {
                  if (clicked.svg_element)
                  {
                      const auto& element = *clicked.svg_element;
                      // Используйте element.id, element.name или element.index.
                  }
              });

window.enable_hit_testing();
```

Та же настройка работает с CPU-объектом `svg_object`; для GPU-экземпляров передайте её в `svg_sprite::from_file()` или загрузите hit-testable `svg_object`, вызовите `upload()`, затем создайте `svg_sprite`. Записи соответствуют рисуемым элементам внутри `<g>`; сам узел группы отдельной областью не становится. Для клонированных через `<use>` элементов сохраняются ID исходной фигуры, а `index` различает её экземпляры. Отверстия и обводки учитываются по фактической треугольной геометрии. Проверка не читает цвет и alpha с GPU.

## Один SVG в разных размерах

Объект хранит геометрию независимо от вывода. Его можно нарисовать несколько раз, меняя transform между синхронными вызовами `frame.draw()`:

```cpp
auto logo = mngl::two_d::svg_object::from_file("assets/logo.svg");
auto frame = window.frame();

logo.transform(mngl::two_d::transform{}.scale(0.01f));
frame.draw(logo);

logo.transform(mngl::two_d::transform{}.translate({1.2f, 0.0f}).scale(0.005f));
frame.draw(logo);

frame.display();
```

Здесь оба вывода используют одну разобранную геометрию. Настройка transform не пересоздаёт документ и не меняет GPU vertex buffer.

## Поддерживаемые SVG

Поддерживаются элементы `path`, `rect`, `circle`, `ellipse`, `line`, `polygon`, `polyline` и группы `g`. Команды `path`: `M`, `L`, `H`, `V`, `C`, `S`, `Q`, `T`, `A`, `Z` в верхнем и нижнем регистре. Применяются вложенные `matrix`, `translate`, `scale`, `rotate`, `skewX`, `skewY`; презентационные атрибуты, CSS-классы из `<style>` и inline `style` поддерживают `fill`, линейные и радиальные градиенты, `stroke`, `color`, `currentColor`, `opacity`, `display`, `visibility`, `stroke-width`, `fill-rule`, `stroke-linecap`, `stroke-linejoin`, `stroke-miterlimit`, `stroke-dasharray` и `stroke-dashoffset`.

Для цветов доступны все 148 непрозрачных именованных sRGB цветов CSS Color 4 без учёта регистра ASCII, `transparent`, контекстный `currentColor`, `rgb()`/`rgba()` и hex-формы `#RGB`, `#RGBA`, `#RRGGBB`, `#RRGGBBAA`. Системные CSS-цвета и функции HSL, HWB, Lab, LCH, Oklab, OkLCh и `color()` пока не поддерживаются.

В таблице стилей поддерживаются простые селекторы по тегу, `*`, `#id`, `.class` и их сочетания, а также селекторы потомков, где пробел означает любой уровень вложенности. Через запятую можно перечислить несколько селекторов:

```xml
<svg viewBox="0 0 40 20">
  <style><![CDATA[
    .accent { fill: #356ae6; }
    path.accent { stroke: #18346f; stroke-width: 1; }
    .toolbar path.accent { fill: #18a058; }
    #muted { fill: currentColor; color: #777; }
  ]]></style>
  <g class="toolbar">
    <path class="accent" d="M 0 0 H 18 V 18 H 0 Z"/>
  </g>
  <path id="muted" class="accent" d="M 22 0 H 40 V 18 H 22 Z"/>
</svg>
```

Селекторы атрибутов поддерживают наличие (`[data-role]`), точное равенство (`=`), список слов (`~=`), дефисный префикс (`|=`), префикс (`^=`), суффикс (`$=`) и подстроку (`*=`). Значения можно заключать в кавычки. Флаг `i` после значения включает нечувствительность к ASCII-регистру, например `[data-kind="icon" i]`; флаг `s` явно задаёт регистрозависимое сравнение. Без флага сравнение регистрозависимое. Атрибутный селектор добавляет специфичность как селектор класса.

Пример условных стилей для широкого SVG:

```xml
<svg width="240px" height="80px">
  <style><![CDATA[
    @media screen and (min-width: 200px) and (orientation: landscape) {
      .wide { fill: #356ae6; }
    }
    @media print {
      .wide { fill: black; }
    }
  ]]></style>
  <rect class="wide" width="20" height="20"/>
</svg>
```


Поддерживаются структурные псевдоклассы `:root`, `:empty`, `:first-child`, `:last-child`, `:only-child`, `:first-of-type`, `:last-of-type`, `:only-of-type`, `:nth-child()`, `:nth-last-child()`, `:nth-of-type()`, `:nth-last-of-type()`. Функции `:nth-*` принимают целые числа, `odd`, `even` и формулы `an+b`. Псевдоклассы `:not()`, `:is()` и `:where()` принимают списки селекторов; специфичность `:is()` и `:not()` берётся от самого специфичного аргумента, а `:where()` не добавляет специфичности. `:lang()` проверяет `lang` или `xml:lang` у элемента и его предков.

Стили элементов группы наследуются дочерними фигурами. Презентационные атрибуты имеют меньший приоритет, чем совпавшие CSS-правила; среди правил учитываются специфичность и порядок в таблице. Inline `style` имеет более высокий приоритет, а `!important` — более высокий приоритет, чем обычное объявление. Поддерживается `display="none"` и CSS-свойства `display`/`visibility`.

Пробел ищет совпадение на любом уровне предков. Поддерживается `@media` с типами `all`, `screen` и `print`, списками через запятую, модификаторами `not`/`only`, вложенными блоками и условиями по `width`, `height`, `orientation` и `aspect-ratio`. Размеры сравниваются в CSS-пикселях; media-правила вычисляются один раз при загрузке по `width`/`height` корневого `<svg>` (по умолчанию 300×150), изменение view после загрузки не пересчитывает CSS. Остальные at-rules, включая `@import`, `@supports` и `@font-face`, пропускаются; внешние таблицы стилей не загружаются. Дочерний комбинатор `>`, комбинаторы соседей `+` и `~`, интерактивные псевдоклассы состояния (`:hover`, `:focus`, `:active` и другие), псевдоэлементы и конструкция `of S` в `:nth-child()` пока не поддерживаются.

Обводка строится векторной треугольной геометрией. `stroke-linecap` принимает `butt`, `round` и `square`; `stroke-linejoin` — `miter`, `round` и `bevel`. Для круглых концов и стыков используется текущий `curve_tolerance`. Порог `stroke-miterlimit` по умолчанию равен `4`. `stroke-dasharray` принимает список чисел или значений с `px`; нечётный список повторяется, `none` и список из одних нулей задают сплошную обводку. `stroke-dashoffset` сдвигает начало рисунка вдоль каждого подконтура. Значения в процентах и единицах, отличных от `px` и пользовательских единиц, не поддерживаются.

Например, пунктир с круглыми концами и фасочными стыками:

```xml
<path d="M 0 10 L 20 0 L 40 10"
      fill="none" stroke="#356ae6" stroke-width="3"
      stroke-linecap="round" stroke-linejoin="bevel"
      stroke-dasharray="6 3" stroke-dashoffset="1"/>
```

Квадратичные, кубические кривые, эллиптические дуги, круги, эллипсы и скруглённые углы прямоугольников адаптивно аппроксимируются линейными сегментами во время CPU-загрузки. Точность готовой геометрии задаётся при загрузке; `max_curve_segments` также ограничивает количество сегментов, создаваемых для штриховки и круглых caps/joins. Превышение лимита приводит к `std::length_error`.

Все под-контуры одного `<path>` участвуют в общей заливке; по умолчанию используется `nonzero`. `fill-rule="evenodd"` переключает заливку при каждом вложенном контуре. `fill-rule="nonzero"` учитывает направление обхода: чтобы вырезать отверстие, внутренний контур должен идти в направлении, противоположном внешнему. Вложенный контур внутри отверстия снова создаёт залитую область. Каждый контур при этом отдельно формирует обводку.

Например, у `evenodd` направление внутреннего контура неважно:

```xml
<path fill-rule="evenodd"
      d="M 0 0 H 100 V 100 H 0 Z M 25 25 H 75 V 75 H 25 Z"/>
```

Для отверстия с правилом `nonzero` внутренний контур обходится в обратную сторону:

```xml
<path fill-rule="nonzero"
      d="M 0 0 H 100 V 100 H 0 Z M 25 25 V 75 H 75 V 25 Z"/>
```

Заливка поддерживает самопересечения и пересечения разных подконтуров. MNGL разбивает плоскость по вершинам и пересечениям рёбер, затем применяет `evenodd` или `nonzero` к получившимся участкам. Та же геометрия используется для проверки попадания.

### Градиенты

Заливка и обводка могут ссылаться на `<linearGradient>` или `<radialGradient>` через `fill="url(#id)"` и `stroke="url(#id)"`. Стопы градиента принимают `offset`, `stop-color` и `stop-opacity` в атрибутах и стилях. Для горизонтальной или вертикальной линии задайте градиенту `gradientUnits="userSpaceOnUse", поскольку у линии одна из сторон геометрических границ равна нулю.

```xml
<svg viewBox="0 0 100 100">
  <defs>
    <linearGradient id="sky" x1="0%" y1="0%" x2="100%" y2="100%"
                    spreadMethod="reflect">
      <stop offset="0%" stop-color="#46b9ff"/>
      <stop offset="100%" stop-color="#4726a8" stop-opacity="0.8"/>
    </linearGradient>
    <radialGradient id="glow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#fff6a8"/>
      <stop offset="100%" stop-color="#ff6b35"/>
    </radialGradient>
  </defs>
  <path d="M 5 5 H 95 V 95 H 5 Z" fill="url(#sky)"/>
  <circle cx="50" cy="50" r="24" fill="url(#glow)" stroke="url(#sky)" stroke-width="3"/>
</svg>
```

Шейдер вычисляет цвет по локальным координатам векторных треугольников. При изменении масштаба используется та же геометрия.

`gradientUnits` поддерживает `objectBoundingBox` (по умолчанию) и `userSpaceOnUse`; также поддерживаются `gradientTransform`, `spreadMethod="pad|repeat|reflect"`, `color-interpolation="sRGB|linearRGB"`, проценты в координатах и радиусы, `fx`, `fy`, `fr` и прозрачность стопов. По умолчанию цветовые стопы интерполируются в sRGB. Для CPU-параллельной записи вызовите `frame.prepare(svg)` до создания worker recording contexts, как и для других frame resources.

Встроенный fragment shader компилируется при первом использовании градиента. Приложению нужны каталог `shaders` рядом с executable и runtime shader tools из поставки MNGL. CMake demo копирует каталог шейдеров, а runtime package включает и его, и shader tools.

### Повторное использование через `<defs>` и `<use>`

Фигуры и группы можно объявлять в `<defs>`, а затем вставлять через `<use href="#id">`. Каждый экземпляр получает собственные `x`/`y`, `transform` и стили. Стили наследуются в группу или фигуру по обычной цепочке; вложенные `<use>` поддерживаются. Например:

```xml
<svg viewBox="0 0 100 50">
  <defs>
    <g id="leaf" stroke="#245c37" stroke-width="2">
      <path d="M 0 10 C 5 0 15 0 20 10 C 15 20 5 20 0 10 Z"/>
      <path d="M 2 10 L 18 10" fill="none"/>
    </g>
  </defs>
  <use href="#leaf" x="5" y="5" fill="#52a66b"/>
  <use href="#leaf" x="45" y="5" transform="scale(1.5)" fill="#e5a33b"/>
</svg>
```

Ссылки должны указывать на фигуру, `<g>` или `<image>` с `id` внутри `<defs>`. `xlink:href` также принимается. Неизвестные ID и циклические ссылки отклоняются с `std::invalid_argument`. Внешние ссылки и ссылки на `<symbol>` или вложенный `<svg>` пока не поддерживаются.

### Маркеры

Ссылки `marker-start`, `marker-mid` и `marker-end` размещают векторный рисунок из `<marker>` в вершинах фигуры. Их можно задать также одним свойством `marker`. Для кривой берутся вершины исходного `path`, а направление определяется по касательным. `orient` принимает угол, `auto` или `auto-start-reverse`. Поддерживаются `markerUnits`, `markerWidth`, `markerHeight`, `refX`, `refY`, `viewBox` и `preserveAspectRatio`. `context-fill` и `context-stroke` могут взять сплошной цвет у фигуры.

~~~xml
<svg viewBox="0 0 40 20">
  <defs>
    <marker id="arrow" viewBox="0 0 6 6" refX="5" refY="3" orient="auto">
      <path d="M0 0 L6 3 L0 6 Z" fill="context-stroke"/>
    </marker>
  </defs>
  <path d="M5 10 H32" stroke="#275db5" marker-end="url(#arrow)"/>
</svg>
~~~

### Изображения

`<image>` принимает PNG, JPEG и WebP из Base64 URI в `href` или `xlink:href`. `preserveAspectRatio` поддерживает выравнивание, `meet`, `slice` и `none`. При `slice` лишние части изображения обрезаются границами элемента.

Чтобы разрешить относительные пути к файлам в каталоге SVG и его подкаталогах, используйте `from_file()` с `svg_load_options::allow_external_images = true`. При загрузке строки доступны встроенные растровые изображения.

~~~cpp
mngl::two_d::svg_load_options options;
options.allow_external_images = true;
auto scene = mngl::two_d::svg_sprite::from_file(window.gpu_device(), "assets/scene.svg", options);
~~~

### Маски и фильтры

`mask="url(#id)"` и `filter="url(#id)"` применяются к фигуре или группе целиком. `<mask>` поддерживает яркостный и альфа режимы через `mask-type`, `maskUnits`, `maskContentUnits` и прямоугольную область `x/y/width/height`. Содержимое маски рисуется тем же SVG рендерером. Если на фигуре есть оба эффекта, сначала применяется фильтр, затем маска.

`<filter>` обрабатывает `SourceGraphic`, `SourceAlpha` и именованные результаты предыдущих шагов через `in`, `in2` и `result`. Поддерживаются `feGaussianBlur`, `feDropShadow`, `feOffset`, `feFlood`, `feColorMatrix`, `feComposite`, `feBlend` и `feMerge`. Доступны `filterUnits`, `primitiveUnits`, области примитивов и выбор `sRGB` или `linearRGB` для фильтра. `feBlend` поддерживает `normal`, `multiply`, `screen`, `darken` и `lighten`.

~~~xml
<svg viewBox="0 0 80 40">
  <defs>
    <mask id="fade" mask-type="alpha" maskContentUnits="objectBoundingBox">
      <rect width="1" height="1" fill="white" fill-opacity=".7"/>
    </mask>
    <filter id="shadow" x="-25%" y="-25%" width="150%" height="150%">
      <feDropShadow dx="2" dy="2" stdDeviation="1.5"/>
    </filter>
  </defs>
  <g filter="url(#shadow)" mask="url(#fade)">
    <rect x="10" y="8" width="52" height="20" fill="#356ae6"/>
  </g>
</svg>
~~~

Размер временных GPU текстур для эффектов берётся из текущего view и размера цели. Длина каждой стороны ограничена 2048 пикселями. Общий для выходных, capture- и промежуточных целей фильтров пул ограничен 64 MiB и 64 GPU-целями. Если цели заняты незавершёнными командами и новый эффект превышает этот бюджет, операция может выбросить `std::length_error`. Например, одна цель 2048×2048 в формате RGBA16F занимает 32 MiB, поэтому фильтр, которому нужны выходная текстура, копия исходного кадра и промежуточная цель, может потребовать около 96 MiB — больше текущего лимита. Исходные фигуры хранятся как геометрия. Для эффектов приложению нужны `svg_filter.vert.hlsl` и `svg_filter.frag.hlsl` из каталога `shaders` и те же инструменты компиляции, что для градиентов. Если рядом есть скомпилированные версии подходящего формата, MNGL загружает их напрямую.

Процентные значения поддерживаются для координат и размеров фигур, изображений, областей эффектов и параметров градиентов. Локальные ссылки `clip-path="url(#id)"` поддерживают `<clipPath>` с `clipPathUnits="userSpaceOnUse"` (по умолчанию) или `objectBoundingBox`; `clip-path="none"` не добавляет отсечение. Геометрия вне clip path не отображается и не участвует в hit testing. Наследование градиентов через `href`, сетевые и SVG изображения, текст, анимация и вложенные SVG viewport пока не поддерживаются. Функции CSS фильтров, множественные маски и примитивы за пределами приведённого списка также пока недоступны.

Каждая заливка и обводка сохраняется отдельной частью геометрии со своим цветом и порядком рисования. Tint и blend объекта применяются к выводу всех частей.

GPU представление объединяет соседние одноцветные части с одинаковым итоговым цветом. Градиентные части хранят свои координаты в отдельных mesh для шейдера. `svg_sprite::geometry_count()` считает верхние части после объединения, включая отдельные изображения и области с эффектами.
