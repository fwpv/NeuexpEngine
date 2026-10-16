rem Отключаем вывод команд оболочки.
@echo off
rem Делаем изменения переменных среды локальными для этого скрипта.
setlocal
rem Переходим в корень проекта: %~dp0 означает папку скрипта, .. — родительскую папку; /d меняет также диск.
cd /d "%~dp0.."
if errorlevel 1 exit /b %errorlevel%

rem Конфигурируем проект из текущей папки (-S .) в папку build (-B build).
rem Явно выбираем указанный пользователем генератор MinGW Makefiles.
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
rem В batch-синтаксисе IF ERRORLEVEL 1 истинно при коде завершения 1 или выше.
rem EXIT /B возвращает ошибку вызывающей оболочке, сохраняя код CMake.
if errorlevel 1 exit /b %errorlevel%

rem MinGW Makefiles использует одну конфигурацию: Release задаётся выше через CMAKE_BUILD_TYPE.
cmake --build build
rem Возвращаем вызывающей оболочке код завершения сборки.
exit /b %errorlevel%
