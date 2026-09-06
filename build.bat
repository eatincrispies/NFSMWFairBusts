@echo off
setlocal

rem Builds build\NFSMWFairBusts.asi with a 32-bit MinGW-w64 g++.
rem The plugin uses no C++ runtime, so -static-libstdc++ is not needed.

set CXX=g++
set CXXFLAGS=-m32 -std=c++17 -O2 -Wall -Wextra -fno-strict-aliasing -fno-exceptions -fno-rtti
set LDFLAGS=-shared -static -static-libgcc -Wl,--exclude-all-symbols -ladvapi32

if not exist build mkdir build

%CXX% %CXXFLAGS% src\Main.cpp src\Game.cpp src\Patch.cpp src\Config.cpp src\Log.cpp %LDFLAGS% -o build\NFSMWFairBusts.asi
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)

echo Built build\NFSMWFairBusts.asi
echo Copy it and NFSMWFairBusts.ini into your Most Wanted "scripts" folder.
endlocal
