@echo off
set "PATH=D:\down\QT\Tools\mingw1310_64\bin;D:\down\QT\Tools\CMake_64\bin;%PATH%"

if not exist build\nul (
    echo === Configuring CMake ===
    cmake -G "MinGW Makefiles" ^
        -DCMAKE_PREFIX_PATH="D:/down/QT/6.11.0/mingw_64" ^
        -DCMAKE_CXX_COMPILER="D:/down/QT/Tools/mingw1310_64/bin/g++.exe" ^
        -DCMAKE_MAKE_PROGRAM="D:/down/QT/Tools/mingw1310_64/bin/mingw32-make.exe" ^
        -B build -S .
)

echo === Building ===
cmake --build build
if %errorlevel% neq 0 exit /b %errorlevel%

echo === Running ===
build\LiJointMaster3.exe
