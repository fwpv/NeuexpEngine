@echo off
rem Ограничиваем изменения переменных среды временем выполнения скрипта.
setlocal
rem Переходим из папки скрипта в корень проекта; /d позволяет сменить также диск.
cd /d "%~dp0.."
if errorlevel 1 exit /b %errorlevel%

rem Отдельный каталог сохраняет настройки обычной сборки демо в build.
cmake -S . -B build/tests-only -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_TESTING=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build build/tests-only
if errorlevel 1 exit /b %errorlevel%

rem Запуск из каталога сборки совместим с минимальной версией CMake проекта.
cd /d "build\tests-only"
if errorlevel 1 exit /b %errorlevel%
rem Показываем вывод проваленных тестов и возвращаем код завершения CTest.
ctest --output-on-failure
exit /b %errorlevel%
