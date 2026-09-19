@echo off
setlocal

rem Builds build\NFSMWFairBusts.asi with a 32-bit MinGW-w64 g++ (C++20).

set CXX=g++
set CXXFLAGS=-m32 -std=c++20 -O2 -Wall -Wextra -fno-strict-aliasing -fno-exceptions -fno-rtti -DNOMINMAX -DWIN32_LEAN_AND_MEAN
set LDFLAGS=-shared -static -static-libgcc -static-libstdc++ -Wl,--exclude-all-symbols -Wl,-u,___mingw_SEH_error_handler -ladvapi32

if not exist build mkdir build

%CXX% %CXXFLAGS% src\dllmain.cpp %LDFLAGS% -o build\NFSMWFairBusts.asi
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)

echo Built build\NFSMWFairBusts.asi
echo Copy it and NFSMWFairBusts.ini into your Most Wanted "scripts" folder.
endlocal
