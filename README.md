# MNGL

MNGL — C++ библиотека, API которой сейчас активно развивается.

## Документация

Исходники руководства находятся в `docs/` и пишутся в Markdown. MkDocs собирает из них статический сайт в `site/`; готовую папку можно целиком загрузить на статический хостинг. Страницы публикуются с адресами вида `2d/quick-start/`, а файлы сайта имеют структуру `site/2d/quick-start/index.html`.

```powershell
python -m pip install -r requirements-docs.txt
python -m mkdocs serve
python -m mkdocs build --strict
```

Для автоматической справки по C++ API позднее можно добавить Doxygen и разместить его результат внутри того же сайта.
