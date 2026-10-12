rem Отключаем вывод команд оболочки.
@echo off
rem Делаем изменения переменных среды локальными для этого скрипта.
setlocal
rem Переходим в каталог скрипта; /d разрешает сменить диск, а %~dp0 означает его папку.
cd /d "%~dp0"

rem Конфигурируем проект из текущей папки (-S .) в папку build (-B build).
rem Явно выбираем указанный пользователем генератор MinGW Makefiles.
cmake -S . -B build -G "MinGW Makefiles"
rem В batch-синтаксисе IF ERRORLEVEL 1 истинно при коде завершения 1 или выше.
rem EXIT /B возвращает ошибку вызывающей оболочке, сохраняя код CMake.
if errorlevel 1 exit /b %errorlevel%

rem Собираем настроенную цель в папке build; --config Release применимо к многоконфигурационным генераторам.
cmake --build build --config Release
rem Возвращаем вызывающей оболочке код завершения сборки.
exit /b %errorlevel%
