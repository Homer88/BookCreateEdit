@echo off
echo === Building BookCreateEdit ===

set MINGW=D:\Qt\Qt5.14.2\Tools\mingw730_64\bin
set QT_PREFIX=D:\Qt\Qt5.14.2\5.14.2\mingw73_64
set PATH=%MINGW%;%PATH%

if not exist build mkdir build
cd build

cmake .. -G "MinGW Makefiles" -DCMAKE_C_COMPILER=%MINGW%\gcc.exe -DCMAKE_CXX_COMPILER=%MINGW%\g++.exe -DCMAKE_PREFIX_PATH=%QT_PREFIX%
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

cmake --build .
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo === Build successful ===
echo Run: build\BookCreateEdit.exe
pause
