#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."

rm -rf -- build build-mingw CMakeFiles
rm -f -- CMakeCache.txt Makefile cmake_install.cmake circle circle.exe
