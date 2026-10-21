@echo off
setlocal
cd /d "%~dp0.."
if errorlevel 1 exit /b %errorlevel%

if exist "build" rmdir /s /q "build"
if exist "build-mingw" rmdir /s /q "build-mingw"
if exist "CMakeFiles" rmdir /s /q "CMakeFiles"
if exist "CMakeCache.txt" del /q "CMakeCache.txt"
if exist "Makefile" del /q "Makefile"
if exist "cmake_install.cmake" del /q "cmake_install.cmake"
if exist "circle.exe" del /q "circle.exe"
