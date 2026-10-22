from mngl_cli import connect_gui
from random import randint
app = connect_gui()
selected = app.selected_tree()

for i in range(1, 1000000):
    selected.insert(randint(1, 1000000))

print(selected.contains(30))

print(selected.contains(100))

print("питон работает =))")