#!/usr/bin/env bash
# Останавливаем очистку при ошибке команды или неопределённой переменной.
set -euo pipefail

# BASH_SOURCE[0] содержит путь скрипта; переходим из его папки в корень проекта.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

# -r удаляет каталоги рекурсивно, -f разрешает отсутствие файлов; -- завершает опции.
rm -rf -- build build-mingw CMakeFiles
# Удаляем также следы сборки, выполненной непосредственно в корне проекта.
rm -f -- CMakeCache.txt Makefile cmake_install.cmake circle circle.exe
