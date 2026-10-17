#!/usr/bin/env bash
# Завершаем скрипт при ошибке команды или обращении к неопределённой переменной.
set -euo pipefail

# BASH_SOURCE[0] содержит путь скрипта; переходим из его папки в корень проекта.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

# Unix Makefiles использует одну конфигурацию, поэтому выбираем Release при настройке.
cmake -S . -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=ON -DBUILD_TESTING=OFF
cmake --build build
