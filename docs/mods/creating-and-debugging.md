# Создание и отладка модов

MNGL v1 загружает доверенные нативные библиотеки через SDL3. Мод получает только те расширения, которые host заранее объявил в своём API: он может оборачивать и заменять функции, задавать override свойств и добавлять фабрики классов. Нативный мод имеет те же права процесса, что и приложение; загрузчик не изолирует его в sandbox.

## Готовый пример

В репозитории есть собираемый host и отдельная динамическая библиотека:

- `examples/mods/host/main.cpp` объявляет дерево BST и точки расширения;
- `examples/mods/shared/tree_api.hpp` — общий интерфейс между host и модом;
- `examples/mods/fast_tree_mod.cpp` — пример мода;
- `MNGL/mods.hpp` и `MNGL/mod_loader.hpp` — публичный API и загрузчик.

Пример мода заменяет слот вставки и нормализует отрицательные ключи, добавляет `around`-обёртку для трассировки, задаёт глобальное свойство с названием дерева и регистрирует фабрику реализации `demo.tree.counting`. Host показывает штатное поведение до загрузки, изменённое поведение и значение свойства после загрузки, а затем штатное поведение после выгрузки.

### Сборка и запуск

Цель собирается по умолчанию. Её можно отключить параметром `-DMNGL_BUILD_MOD_EXAMPLE=OFF`.

Windows с Visual Studio:

```powershell
cmake -S . -B build-mods -A x64 -DMNGL_BUILD_MOD_EXAMPLE=ON
cmake --build build-mods --config Debug --target mngl_mod_host
cmake --build build-mods --config Debug --target mngl_example_tree_mod
& .\build-mods\mod-demo\Debug\mngl_mod_host.exe
```

Linux с Ninja (при установленном C++23 toolchain и SDL3 development package):

```sh
cmake -S . -B build-mods -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMNGL_BUILD_MOD_EXAMPLE=ON
cmake --build build-mods --target mngl_mod_host
cmake --build build-mods --target mngl_example_tree_mod
./build-mods/mod-demo/Debug/mngl_mod_host
```

Корневой CMake-проект также требует SDL3_image и FreeType development packages, потому что эти зависимости нужны `mngl_demo`.

Выходная папка содержит host, примерную библиотеку и пакет мода:

```text
build-mods/mod-demo/Debug/
├── mngl_mod_host[.exe]
├── module-build/
│   └── example_tree_tools[.dll|.so|.dylib]
└── mods/
    └── example.tree-tools/
        ├── mod.json       # создаётся host перед запуском
        └── bin/
            └── example_tree_tools[.dll|.so|.dylib]
```

Цели host и мода независимы. Сначала соберите `mngl_mod_host`, затем отдельно `mngl_example_tree_mod`: сборка мода копирует только его библиотеку в каталог пакета рядом с host-ом. Запуск host формирует `mod.json`, подставляет точный результат `native_binary_abi_tag()` текущей сборки и вызывает `mod_loader::load_all()`. Для обновления кода мода достаточно снова собрать `mngl_example_tree_mod` и запустить существующий host; пересборка приложения не нужна. Пакет работает в Debug и Release без ручного изменения ABI-поля. Проверить тег сборки можно командой:

```powershell
& .\build-mods\mod-demo\Debug\mngl_mod_host.exe --print-abi
```

При запуске host выводит три фазы. До загрузки `-5` и `5` — разные ключи. Во время действия `example.tree-tools` мод заменяет функцию вставки: эти ключи становятся одинаковыми, меняется название дерева, а `around`-слой пишет трассировку. При выгрузке host уничтожает созданные модом объекты, собирает отложенную выгрузку и повторяет вставки: штатное поведение и исходное название дерева возвращаются.

### Интеграционная проверка

CTest собирает отдельный конфликтующий мод и проверяет реальную загрузку библиотек. Проверка охватывает повреждённый manifest, несовпадающий ABI, успешную замену функции и свойства, диагностику конкурирующей замены, восстановление host fallback и отложенную выгрузку, пока жив экземпляр класса из мода.

```powershell
cmake --build build-mods --config Debug --target mngl_mod_integration_test
ctest --test-dir build-mods -C Debug --output-on-failure
```

GitHub Actions запускает эту проверку на Windows при push и pull request.

Для подготовки локальной среды модов и контракта CI/CD см. [инструкцию по автоматизации](automation.md).

Для проверки другого пакета передайте каталог, который непосредственно содержит подпапки-пакеты с `mod.json`:

```powershell
& .\build-mods\mod-demo\Debug\mngl_mod_host.exe C:\path\to\my-mods
```

Когда задан каталог аргументом, host не генерирует manifest в нём. Загрузчик рассматривает каждую непосредственную подпапку как отдельный пакет и продолжает работу после ошибки отдельного мода.

## Структура собственного мода

Мод — это динамическая библиотека с C entrypoint-ами и manifest рядом с бинарным файлом. В manifest задайте постоянный `id`, версию мода, требуемую версию API, ABI-тег host и относительный путь к библиотеке:

```json
{
  "schema_version": 1,
  "id": "studio.my-mod",
  "name": "My MNGL mod",
  "version": "1.0.0",
  "api_version": "1.0",
  "binary_abi": "<значение, полученное от host --print-abi>",
  "binary": "bin/my_mod.dll",
  "dependencies": {
    "studio.base-tools": ">=1.2.0 <2.0.0"
  }
}
```

Используйте расширение текущей платформы (`.dll`, `.so` или `.dylib`). `binary` всегда задаётся относительно каталога пакета через `/`; загрузчик отклоняет абсолютные пути, `.` и `..`, а также путь, который после разрешения symlink выходит за пределы пакета.

Поле `dependencies` можно опустить, если моду не нужны другие моды. Иначе перечислите постоянные `mod_id` и поддерживаемые версии. Диапазон — это `*` либо условия `=`, `<`, `<=`, `>` и `>=` с версиями `MAJOR.MINOR.PATCH`, разделённые пробелами и объединённые через AND: `">=1.2.0 <2.0.0"`. При `load_all()` загрузчик сам ставит пакеты в правильный порядок; при `load_package()` зависимости должны быть загружены заранее. Отсутствующая зависимость и неподходящая версия указываются в ошибке. Цикл приводит к отказу всех его участников, а сообщение показывает цепочку ID; моды, зависящие от цикла, сообщают, что заблокированы им.

Загруженный мод-провайдер остаётся в памяти, пока активен использующий его мод. `unload(provider_id)` вернёт `dependency_in_use`, если зависимый мод ещё загружен или ждёт безопасной выгрузки. `unload_all()` выгружает моды в обратном порядке зависимостей. Если выгрузка зависимого мода отложилась из-за созданного им объекта, уничтожьте объект, вызовите `collect_deferred()`, затем повторите `unload_all()`.

Простейшая точка входа:

```cpp
#include <mngl/mod_loader.hpp>

extern "C" MNGL_MOD_EXPORT bool MNGL_MOD_CALL mngl_mod_load_v1(mngl::mods::mod_context& context)
{
	context.around_function<bool(tree_base&, int)>("demo.tree.insert", 10,
	                                               [](auto next, tree_base& tree, int key)
	                                               {
		                                               // Изменить аргументы, вызвать next один раз и
		                                               // обработать результат.
		                                               return next(tree, key);
	                                               });
	return true;
}

extern "C" MNGL_MOD_EXPORT void MNGL_MOD_CALL mngl_mod_unload_v1()
{
	// Остановить потоки или освободить состояние мода.
}
```

`mngl_mod_load_v1` обязателен. Если он возвращает `false` или выбрасывает исключение, загрузчик откатывает регистрации. `mngl_mod_unload_v1` необязателен и вызывается после завершения callbacks и уничтожения объектов, созданных фабриками этого мода. Исключения не должны пересекать границу C entrypoint-а.

Для новой реализации базовый интерфейс должен наследовать `mngl::mods::modable`, а фабрика должна возвращать `std::unique_ptr<Base>`:

```cpp
context.add_implementation<tree_base>("demo.tree", "demo.tree.my-tree",
                                      [] { return std::make_unique<my_tree>(); });
```

Установка свойства использует объявленный host-ом ID и его точный тип:

```cpp
context.set_property<std::string>("demo.tree.label", "My tree");
```

Настройки можно менять и сохранять через реестр host-а. Загружайте их после загрузки модов, чтобы overrides уже были зарегистрированы; перед выгрузкой модов сохраните текущие значения. В v1 автоматически сохраняются глобальные и class-default overrides типов `bool`, целых фиксированной ширины, `float`, `double` и `std::string`. Значения instance-свойств и произвольные типы остаются временными.

Пример demo-host предоставляет простой консольный интерфейс и хранит значения в `mod-settings.ini` рядом с исполняемым файлом:

```powershell
& .\mngl_mod_host.exe --list-settings
& .\mngl_mod_host.exe --set-setting example.tree-tools demo.tree.node-gap 36.5
& .\mngl_mod_host.exe --set-setting example.tree-tools demo.tree.label "My tree"
```

Собственный host может построить графический интерфейс по `registry.settings()` и записывать значения через `registry.set_setting()`. Метод проверяет тип и валидатор свойства; `registry.save_settings(path)` сохраняет текущие overrides.

Нельзя вызвать или переопределить произвольный метод класса. Host должен зарегистрировать слот через `REGISTER_MODDABLE_FUNCTION` или `REGISTER_MODDABLE_FUNCTION_AS`, свойство — через `REGISTER_MODDABLE_*_PROPERTY`, а класс — через `REGISTER_MODDABLE_CLASS` и `REGISTER_MODDABLE_FACTORY`. После регистрации host вызывает `registry.apply_declarations()` и направляет обращения через слоты реестра.

## Сборка библиотеки мода

Добавьте в CMake отдельную цель `MODULE` или `SHARED`, используйте C++23, тот же компилятор, архитектуру и конфигурацию, что и host, и направьте вывод в структуру пакета:

```cmake
set(MNGL_SOURCE_DIR "/path/to/MNGL" CACHE PATH "MNGL source checkout")
add_library(my_mngl_mod MODULE src/my_mod.cpp)
target_compile_features(my_mngl_mod PRIVATE cxx_std_23)
target_include_directories(my_mngl_mod PRIVATE
    "${MNGL_SOURCE_DIR}/MNGL"
    "${MNGL_SOURCE_DIR}/include")
set_target_properties(my_mngl_mod PROPERTIES
    PREFIX ""
    OUTPUT_NAME "my_mod")
```

Скопируйте бинарник в `my-mod/bin/`, а manifest — в `my-mod/mod.json`. В проекте, собранном из исходников MNGL, пример host и модуль показывают настройку include paths и экспортируемых entrypoint-ов. При подключении установленного CMake-пакета используйте `MNGL::MNGL` для типов MNGL и настройте include path к `mod_loader.hpp`.

Мод обязан совпадать с host по ABI, C++ runtime и конфигурации сборки. Для MSVC отдельно важны разрядность, выбор `/MD` или `/MT`, `_ITERATOR_DEBUG_LEVEL` и Debug/Release. ABI-тег от `native_binary_abi_tag()` отсекает известные несовпадения, но не доказывает совместимость произвольных compiler flags. Собирайте мод тем же toolchain и не передавайте через ABI-границу STL-объекты в C entrypoint-ах: C ABI используется только для точек загрузки, после чего общая C++ API должна быть собрана совместимо.

## Отладка

В Visual Studio выберите `mngl_mod_host` как startup target. Поставьте breakpoint в `mngl_mod_load_v1`, callback-е `around_function` или фабрике мода, затем запустите host под debugger-ом. Примерный мод собирается вместе с host; PDB остаются рядом со своими DLL в дереве сборки. Для собственного пакета передайте host-у каталог модов как аргумент. На Linux можно запускать host из GDB/LLDB и ставить breakpoint по имени экспортированной функции или функции callback-а.

Сначала смотрите строку `Rejected <mod_id>: ...` в stderr: она содержит причину отклонения manifest, ABI-тега или загрузки библиотеки. Частые случаи:

| Сообщение или симптом | Что проверить |
| --- | --- |
| `mods path is not a directory` | Путь аргумента должен существовать и содержать каталоги пакетов. |
| `mod.json` не найден для пакета | Manifest лежит прямо в папке конкретного мода. |
| `binary_abi does not match host ABI tag` | Пересоберите мод тем же компилятором, runtime, разрядностью и конфигурацией; сверьте `--print-abi`. |
| `cannot load mod binary` | Проверьте путь, формат DLL/SO и наличие runtime-зависимостей библиотеки. |
| `does not export mngl_mod_load_v1` | Entry point должен иметь `extern "C"`, `MNGL_MOD_EXPORT` и `MNGL_MOD_CALL`. |
| `mngl_mod_load_v1 returned false` | Проверьте регистрации ID и типы слотов/свойств, от которых зависит мод. |
| `unload` вернул `deferred` | Уничтожьте мод-созданные объекты и дождитесь завершения callbacks, затем вызовите `collect_deferred()` в синхронизированной точке host-а. |

Не выгружайте DLL вручную через SDL или системный API: загрузчик должен закрывать библиотеку после того, как её код перестал использоваться. Мод с фоновыми потоками должен остановить и присоединить их в `mngl_mod_unload_v1`.
