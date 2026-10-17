# Сторонние библиотеки

Исходники поставляются вместе с проектом. Конфигурация и сборка не скачивают
пакеты, не используют FetchContent и не требуют инициализации Git submodules.

| Библиотека | Версия | Зафиксированный commit | Лицензия |
| --- | --- | --- | --- |
| [stb_image](https://github.com/nothings/stb) | 2.30 | `2c980bb59875b0d32144a71867fbdebb2f77cd20` | MIT или Public Domain; выбран MIT |
| [miniaudio](https://github.com/mackron/miniaudio) | 0.11.23 | `f40cf03f80cdb7e741d43e53b7e706e8c1394bcf` | MIT No Attribution (MIT-0) или Public Domain; выбран MIT-0 |

Оригинальные тексты обеих альтернатив лицензий сохранены в `stb/LICENSE`
и `miniaudio/LICENSE`. Заголовки загружены без изменений из указанных коммитов:

- `https://raw.githubusercontent.com/nothings/stb/2c980bb59875b0d32144a71867fbdebb2f77cd20/stb_image.h`
- `https://raw.githubusercontent.com/mackron/miniaudio/f40cf03f80cdb7e741d43e53b7e706e8c1394bcf/miniaudio.h`

## Проверка целостности

SHA-256 загруженных файлов:

```text
594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3  stb/stb_image.h
bebfe904b14301657e4e5d655c811d51fd31b97c455b9cc2d8600d6bac6cff63  stb/LICENSE
7e4f3f13c8fe66df2080ac3dd12a89193e3c2463cb7f067c798abd7331cd8ee6  miniaudio/miniaudio.h
457f1b500e0adf6bc059edddfa78a2f62012e7c3bb43476c20e0bd23b25ba0eb  miniaudio/LICENSE
```
