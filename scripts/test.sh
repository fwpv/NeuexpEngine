#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

cmake -S . -B build/tests-only -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_TESTING=ON
cmake --build build/tests-only

cd -- build/tests-only
ctest --output-on-failure
