@echo off
setlocal
cd /d "%~dp0.."
if errorlevel 1 exit /b %errorlevel%

cmake -S . -B build/tests-only -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_DEMOS=OFF -DBUILD_TESTING=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build build/tests-only -j 4
if errorlevel 1 exit /b %errorlevel%

cd /d "build\tests-only"
if errorlevel 1 exit /b %errorlevel%
ctest --output-on-failure
exit /b %errorlevel%
