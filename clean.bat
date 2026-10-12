rem Отключаем вывод каждой команды; сообщения самих команд и ошибки останутся видны.
@echo off
rem Ограничиваем действие set командами этого batch-файла.
setlocal
rem Переходим в папку скрипта; /d разрешает сменить также букву диска.
cd /d "%~dp0"

rem Удаляем каталог обычной сборки вместе со всем содержимым (/s) без вопросов (/q).
rem IF EXIST проверяет наличие пути перед удалением; имена путей относительны папке скрипта.
if exist "build" rmdir /s /q "build"
rem Удаляем каталог сборки MinGW, если он существует.
if exist "build-mingw" rmdir /s /q "build-mingw"
rem Удаляем служебный каталог CMake, созданный непосредственно в корне проекта.
if exist "CMakeFiles" rmdir /s /q "CMakeFiles"
rem Удаляем файлы конфигурации, оставшиеся от генерации CMake в корне.
rem CMakeCache.txt содержит кэш параметров конфигурации.
rem Параметр /q удаляет файл без запроса подтверждения.
if exist "CMakeCache.txt" del /q "CMakeCache.txt"
rem Makefile — файл сборки, который мог оставить генератор Unix Makefiles.
if exist "Makefile" del /q "Makefile"
rem cmake_install.cmake — сгенерированный сценарий установки CMake.
if exist "cmake_install.cmake" del /q "cmake_install.cmake"
rem Удаляем исполняемый файл в корне проекта, если он был собран туда.
if exist "circle.exe" del /q "circle.exe"
