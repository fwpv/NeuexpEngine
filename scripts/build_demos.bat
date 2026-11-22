@echo off
setlocal
cd /d "%~dp0.."
if errorlevel 1 exit /b %errorlevel%

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=ON -DBUILD_TESTING=OFF
if errorlevel 1 exit /b %errorlevel%

cmake --build build -j 4
exit /b %errorlevel%
