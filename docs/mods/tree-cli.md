# CLI управления деревьями

`mngl_demo` принимает команды управления деревьями, если запущен с аргументами.
Без аргументов приложение открывает графический редактор, а `--mngl-demo`
запускает отдельное демо MNGL.

CLI загружает алгоритмы модов из папки `mods` рядом с исполняемым файлом.
В список попадают встроенный BST и только успешно загруженные моды.

## Список алгоритмов

```text
mngl_demo tree.types
```

Команда показывает ID алгоритма, отображаемое имя и типы ключей.
Для пустого дерева CLI выбирает тип ключа так же, как GUI: `integer`, затем
`number`, `text` или `big-integer` — в зависимости от возможностей алгоритма.

## Создание файла дерева

```text
mngl_demo tree.create <algorithm-id> <file>
```

Например, создать пустой BST и затем открыть его в GUI:

```powershell
& $exe tree.create mngl.tree.bst .\empty.tree
```

```sh
"$exe" tree.create mngl.tree.bst ./empty.tree
```

Файл содержит сериализацию дерева в формате практикума. Папка назначения
должна существовать. Существующий файл не перезаписывается без `--force`.
Команда возвращает `tree_id` и код `0`; ошибки команды
возвращают ненулевой код. `tree_id` действует до завершения процесса;
для последующих одиночных вызовов передавай путь к файлу.
Передавай `--json` перед командой, чтобы получить JSON-ответ:

```powershell
& $exe --json tree.create mngl.tree.bst .\empty.tree
$exitCode = $LASTEXITCODE
```

```sh
"$exe" --json tree.create mngl.tree.bst ./empty.tree
status=$?
```

Для явной замены уже существующего файла добавь `--force`:

```powershell
& $exe tree.create mngl.tree.bst .\empty.tree --force
```

```sh
"$exe" tree.create mngl.tree.bst ./empty.tree --force
```

## Чтение дерева

`tree.get.file <file_name>` читает файл, распознаёт встроенный BST или алгоритм
из загруженного мода, проверяет сериализацию и инварианты, затем выбирает это
дерево в текущем потоке команд. В ответе есть `tree_id` для последующих команд
в этом же процессе. Размер входного файла ограничен 256 МиБ.

```powershell
& $exe tree.get.file .\empty.tree
```

```sh
"$exe" tree.get.file ./empty.tree
```

`tree.get.selected` выводит метаданные и сериализацию последнего созданного
или прочитанного дерева. Выбор существует в рамках одного процесса, поэтому
для последовательного чтения и последующих запросов используй `--console`:

```powershell
@(
    'tree.get.file .\empty.tree'
    'tree.get.selected'
    ':quit'
) | & $exe --console
```

```sh
printf 'tree.get.file ./empty.tree\ntree.get.selected\n:quit\n' |
    "$exe" --console
```

Для одного вызова `tree.get.file` поток не обязателен. `tree.get.selected`
без ранее выбранного дерева завершится сообщением, что дерево не выбрано.

`tree.open <file>` — короткий алиас `tree.get.file`. `tree.list` перечисляет
деревья, открытые в текущем потоке команд; ответ содержит session ID, алгоритм,
тип ключей и путь. `tree.files <directory>` выводит `.tree` файлы только из
указанной папки, без рекурсивного обхода. `tree.select <tree_id_or_file>` меняет
выбранное дерево, а `tree.close <tree_id_or_file>` закрывает запись только в
текущем процессе.

```powershell
@(
    'tree.open .\empty.tree'
    'tree.list'
    'tree.select tree-1'
    'tree.close tree-1'
    ':quit'
) | & $exe --console
```

```sh
printf 'tree.open ./empty.tree\ntree.list\ntree.select tree-1\ntree.close tree-1\n:quit\n' |
    "$exe" --console
```

### Файлы деревьев

`tree.save-as <tree_id_or_file> <destination>` сохраняет копию в новый файл и
возвращает новый session ID. `tree.rename` переименовывает открытый файл,
сохраняя его session ID. Обе команды отказываются перезаписывать существующий
путь. `tree.delete <tree_id_or_file>` удаляет проверенный `.tree` файл и закрывает
его в сессии; это необратимое файловое действие. Эти команды работают с файлами
деревьев, а не с архивами рабочих областей `.stree`/`.tar`.

```powershell
& $exe tree.save-as .\empty.tree .\copy.tree
& $exe tree.rename .\copy.tree .\renamed.tree
& $exe tree.delete .\renamed.tree
```

```sh
"$exe" tree.save-as ./empty.tree ./copy.tree
"$exe" tree.rename ./copy.tree ./renamed.tree
"$exe" tree.delete ./renamed.tree
```

## Поиск, вставка и удаление

`tree.find`, `tree.insert` и `tree.erase` принимают `tree_id` или путь к файлу
дерева. Используй session ID в `--console`; путь подходит для одиночного
вызова из оболочки. Вставка и удаление сразу сохраняют файл. Поиск у алгоритма
Splay тоже может изменить дерево, поэтому CLI сохраняет файл, если алгоритм
сообщил об изменении.

```powershell
& $exe tree.insert .\empty.tree 42
& $exe tree.find .\empty.tree 42
& $exe tree.erase .\empty.tree 42
```

```sh
"$exe" tree.insert ./empty.tree 42
"$exe" tree.find ./empty.tree 42
"$exe" tree.erase ./empty.tree 42
```

Ключ разбирается по типу, указанному в файле дерева. Для текстовых ключей с
пробелами передавай аргумент в кавычках.

### Случайная вставка

`tree.insert-random <tree_id_or_file> <count>` добавляет уникальные случайные
ключи. Для воспроизводимого результата укажи seed:
`tree.insert-random.seeded <tree_id_or_file> <count> <seed>`. Число ключей должно
быть от 0 до 1 000 000.

```powershell
& $exe tree.insert-random.seeded .\empty.tree 100 12345
```

```sh
"$exe" tree.insert-random.seeded ./empty.tree 100 12345
```

### Слияние

`tree.merge <destination> <source>` объединяет источник с деревом назначения.
Алгоритм проверяет совместимость и свои ограничения слияния. Источник
остаётся без изменений.

```powershell
& $exe tree.merge .\destination.tree .\source.tree
```

```sh
"$exe" tree.merge ./destination.tree ./source.tree
```

### Разделение

`tree.split <tree_id_or_file> <pivot> <left_file> <right_file>` сохраняет ключи
меньше pivot в левый файл, а ключи, равные pivot или больше него, — в правый.
Исходное дерево сохраняется. Ответ содержит session ID обеих новых записей.
Файлы назначения должны отсутствовать.

```powershell
& $exe tree.split .\input.tree 50 .\below.tree .\at-or-above.tree
```

```sh
"$exe" tree.split ./input.tree 50 ./below.tree ./at-or-above.tree
```

## Инварианты и команды модов

`tree.invariants <tree_id_or_file>` запускает инспектор, зарегистрированный для
алгоритма. Для BST встроенный инспектор проверяет порядок ключей и связность.
Для остальных типов используется инспектор загруженного мода. Если у алгоритма
его нет, ответ сообщает об этом; само дерево всё равно прошло валидацию при
загрузке. Код выхода `tree.invariants` равен `1`, если проверка нашла нарушение.

`tree.node <tree_id_or_file> <index>` выводит описание узла от инспектора
алгоритма. Индекс узла начинается с нуля.

```powershell
& $exe tree.invariants .\input.tree
& $exe tree.node .\input.tree 0
```

```sh
"$exe" tree.invariants ./input.tree
"$exe" tree.node ./input.tree 0
```

CLI читает общие описания из `data/tree_commands.json` и описания модов из
`mods/<package>/commands.json`. `tree.commands <tree_id_or_file>` показывает
доступные собственные команды мода с их сигнатурами и типами параметров.
`tree.custom <tree_id_or_file> <command_id> <arguments_json>` запускает такую
команду; аргументы передаются JSON-массивом и проверяются по описанию. После
изменяющей команды CLI проверяет структуру и сохраняет файл.

```powershell
& $exe tree.commands .\trie.tree
& $exe tree.custom .\trie.tree prefix_search '["10"]'
```

```sh
"$exe" tree.commands ./trie.tree
"$exe" tree.custom ./trie.tree prefix_search '["10"]'
```

Параметры в JSON должны соответствовать типам из каталога: строки для `text` и
`bit_string`, целые числа для `int64`, JSON-числа для `double`, `true`/`false`
для `boolean`. Команда доступна только после загрузки мода, его обработчика и
каталога с описанием.

## Поток команд

`--console` выполняет последовательность команд в одном процессе. `tree_id`
остаётся действительным до завершения потока, а `tree.status` показывает
метаданные последнего выбранного дерева.
Поток завершается на `:quit` или `:exit`.

```powershell
@(
    'tree.create mngl.tree.bst .\empty.tree'
    'tree.insert tree-1 42'
    'tree.find tree-1 42'
    'tree.erase tree-1 42'
    'tree.status'
    ':quit'
) | & $exe --console
$exitCode = $LASTEXITCODE
```

```sh
printf 'tree.create mngl.tree.bst ./empty.tree\ntree.insert tree-1 42\ntree.find tree-1 42\ntree.erase tree-1 42\ntree.status\n:quit\n' |
    "$exe" --console
status=$?
```

Справка доступна через `mngl_demo --help` и `mngl_demo tree.create --help`.
