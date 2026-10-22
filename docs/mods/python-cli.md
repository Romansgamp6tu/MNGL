# Python-клиент для CLI деревьев

Пакет `mngl-cli` позволяет из Python вызывать команды `mngl_demo`, создавать
деревья и управлять ими, сохраняя состояние между командами. Пакет не требует
сторонних библиотек во время работы. Для запуска нужен собранный `mngl_demo`;
алгоритмы из модов должны быть разложены рядом с ним так же, как при запуске из
оболочки.

## Установка из репозитория

Из корня репозитория:

```powershell
python -m pip install -e .\python
```

```sh
python -m pip install -e ./python
```

Укажи расположение приложения через переменную окружения `MNGL_DEMO_PATH` либо
передавай его явно при создании клиента:

```powershell
$env:MNGL_DEMO_PATH = (Resolve-Path .\build\Debug\mngl_demo.exe).Path
```

```sh
export MNGL_DEMO_PATH="$PWD/build/mngl_demo"
```

Клиент также ищет `mngl_demo` в `PATH` и в нескольких обычных папках сборки
репозитория. Пути к файлам деревьев разрешаются относительно текущей папки
Python-процесса; её можно задать параметром `cwd`.

## Работа из скрипта

```python
from mngl_cli import MNGL

with MNGL().session() as cli:
    tree = cli.create_tree("mngl.tree.bst", "demo.tree")
    tree.insert(40)
    tree.insert(20)
    tree.insert(60)
    print(tree.find(20).stdout)
    print(tree.invariants().stdout)
```

`TreeSession` держит один процесс CLI, поэтому `tree_id` продолжает работать
между вызовами. Возвращённый объект `Tree` предоставляет методы `insert`,
`find`, `erase`, `insert_random`, `merge`, `split`, `save_as`, `rename`,
`delete`, `invariants`, `node`, `commands` и `custom`. Любую команду можно
вызвать напрямую через сессию:

```python
result = cli.run("tree.types")
if result.ok:
    print(result.stdout)
```

`CommandResult` содержит `exit_code`, `stdout`, `stderr`, а также удобные
`fields` и `value("tree_id")` для простых строк `key=value`. Передай `check=True`
в `run`, чтобы при ненулевом коде получить `MNGLCommandError` с исходным
ответом в `error.result`.

В репозитории есть сквозной пример с Splay:

```powershell
python .\python\examples\splay_demo.py --exe .\build\Debug\mngl_demo.exe
```

```sh
python ./python/examples/splay_demo.py --exe ./build/mngl_demo
```

Splay должен быть загружен как мод. Пример сохраняет `splay-demo.tree` в текущую
папку, вставляет несколько ключей, выполняет поиск и проверку инвариантов. Чтобы
перезаписать созданный ранее файл, добавь `--force`. После скрипта открой `.tree`
в окне MNGL, чтобы увидеть визуальное состояние.

## Интерактивная Python-консоль

Вместо встраивания редактора в приложение пакет открывает обычную интерактивную
консоль Python. Для редактирования многострочных скриптов можно использовать
любой привычный редактор, а затем запускать файл через Python.

```powershell
python -m mngl_cli --exe .\build\Debug\mngl_demo.exe
```

```sh
python -m mngl_cli --exe ./build/mngl_demo
```

В консоли `cli` — уже открытая постоянная сессия, `mngl` — клиент для отдельных
одноразовых команд. Например:

```python
tree = cli.create_tree("mngl.tree.bst", "interactive.tree")
tree.insert(12)
tree.insert(7)
tree.find(7)
```

Tab-автодополнение подключается через стандартный `readline`, если такой модуль
доступен в используемом Python-интерпретаторе.

## Управление открытой вкладкой с анимацией

GUI MNGL также принимает локальные команды от Python. Операция выполняется в
основном потоке интерфейса через те же обработчики, что кнопки приложения:
обновляется открытое дерево, вкладка становится активной, изменения попадают в
историю и воспроизводится анимация, предусмотренная для выбранного дерева и
операции. Ответ ждёт отображения результата и завершения анимации, если она
есть, чтобы следующая команда не прервала её. Изменения остаются
несохранёнными до сохранения дерева в самом приложении.

Для обычного Python-кода используй короткий API:

```python
from mngl_cli import connect_gui

with connect_gui() as MNGL:
    tree = MNGL.selected_tree()
    if tree is not None:
        tree.add(42)
        print(42 in tree)  # True

    for tree in MNGL.opened_trees():
        print(tree.name, tree.algorithm_id, tree.is_dirty)

    for tree in MNGL.loaded_trees():
        print(tree.name, "open:", tree.is_open)
```

`selected_tree()` возвращает активное дерево или `None`. `opened_trees()` даёт
открытые вкладки, а `loaded_trees()` — все `.tree` файлы рабочей области,
включая закрытые вкладки. Вызов метода у дерева из `loaded_trees()` сам откроет
и выберет его. У дерева доступны `contains(key)`, `key in tree`, `add(key)`,
`remove(key)`, `insert(key)`, `erase(key)` и `select()`. Строки автоматически
экранируются: можно писать `tree.contains("hello")` без кавычек синтаксиса
дерева. `add` и `remove` возвращают `True`, если дерево изменилось; `insert`,
`erase` и `find` возвращают подробный `CommandResult`.

Сначала запусти MNGL и открой рабочую область с деревьями. Для длинного скрипта
используй `connect_gui()` в файле `.py`; для пошаговой работы открой живую
консоль:

Для интерактивного Python REPL можно подключиться прямо к открытому окну:

```powershell
python -m mngl_cli --gui --exe .\x64\Debug\MNGL.exe
```

```sh
python -m mngl_cli --gui --exe ./build/mngl_demo
```

В этой консоли `MNGL` уже подключён к окну. Например:

```python
tree = MNGL.selected_tree()
for tree in MNGL.opened_trees():
    print(tree.name, 42 in tree)
```

Если одновременно запущено несколько окон MNGL, укажи выбранный сеанс через
`connect_gui(session_id="...")` или флаг `--session-id`. Сеанс уникален для
каждого окна; его ID можно посмотреть в сообщении `Live Python bridge session
is ready (ID ...)` в журнале приложения. Новые сборки записывают PID процесса,
поэтому Python пропускает записи сеансов, оставшиеся после аварийного закрытия.

В обычной CLI-консоли, запущенной без `--gui`, файл открывается через
`tree = cli.open_tree("demo.tree")`. В GUI-консоли выбирай дерево из
`MNGL.loaded_trees()`. Подключение к окну закрывается при выходе из Python-консоли.

## Проверки клиента и GUI-моста

Python-клиент проверяется стандартным `unittest`, C++-формат обмена — отдельным
CTest. Чтобы CMake добавил Python-набор в CTest, укажи интерпретатор при
конфигурации:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DPython3_EXECUTABLE="C:/Python314/python.exe"
cmake --build build --config Debug --target mngl_python_bridge_protocol_test
ctest --test-dir build -C Debug -R "mngl_python_" --output-on-failure
```

Если CMake не обнаружил Python, тесты клиента можно запустить напрямую:

```powershell
$env:PYTHONPATH = (Join-Path $PWD "python")
python -m unittest discover -s python/tests -v
```
