@echo off
echo Setting up MSYS2 MinGW64 development environment for CFPing...
echo.

REM Check if MSYS2 is installed
if not exist "d:\msys64\mingw64\bin\gcc.exe" (
    echo ERROR: MSYS2 MinGW64 not found at d:\msys64
    echo Please install MSYS2 from https://www.msys2.org/
    echo.
    echo After installation, run these commands in MSYS2 terminal:
    echo pacman -S mingw-w64-x86_64-toolchain
    echo pacman -S mingw-w64-x86_64-cmake
    echo pacman -S mingw-w64-x86_64-qt5-base
    echo pacman -S mingw-w64-x86_64-qt5-tools
    echo pacman -S mingw-w64-x86_64-boost
    pause
    exit /b 1
)

echo MSYS2 MinGW64 found at C:\msys64
echo.

REM Check for required packages
echo Checking for required packages...

if not exist "d:\msys64\mingw64\lib\cmake\Qt5\Qt5Config.cmake" (
    echo WARNING: Qt5 development files not found
    echo Run: pacman -S mingw-w64-x86_64-qt5-base mingw-w64-x86_64-qt5-tools
)

if not exist "d:\msys64\mingw64\include\boost\asio.hpp" (
    echo WARNING: Boost libraries not found
    echo Run: pacman -S mingw-w64-x86_64-boost
)

if not exist "d:\msys64\mingw64\bin\cmake.exe" (
    echo WARNING: CMake not found
    echo Run: pacman -S mingw-w64-x86_64-cmake
)

echo.
echo Setup complete! You can now:
echo 1. Open the project folder in VSCode
echo 2. Use Ctrl+Shift+P and run "CMake: Configure"
echo 3. Use Ctrl+Shift+P and run "CMake: Build"
echo 4. Use F5 to debug or Ctrl+F5 to run
echo.
pause
