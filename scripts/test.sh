#!/usr/bin/env bash
# Останавливаем выполнение при ошибке, включая провал сборки или тестов.
set -euo pipefail

# Переходим из папки скрипта в корень проекта независимо от текущей папки терминала.
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

# Отдельный каталог сохраняет настройки обычной сборки демо в build/.
cmake -S . -B build/tests-only -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_TESTING=ON
cmake --build build/tests-only

# Запуск из каталога сборки поддерживается начиная с минимальной версии CMake проекта.
# --output-on-failure показывает вывод только тех тестов, которые завершились ошибкой.
cd -- build/tests-only
ctest --output-on-failure
