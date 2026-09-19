# NFSMW Fair Busts - 32-bit ASI plugin
#
#   mingw32-make                     build build/NFSMWFairBusts.asi
#   mingw32-make clean
#   mingw32-make install GAME="C:/path/to/NFSMW"
#
# Requires a 32-bit MinGW-w64 g++ (i686-w64-mingw32) with C++20 support. The
# runtime is linked statically, so the .asi needs nothing installed alongside it.
# Structured exception handling (__try/__except) is MSVC-only; the MinGW build
# falls back to VirtualQuery-based pointer validation. For MSVC see README.md.

CXX      ?= g++
TARGET   := build/NFSMWFairBusts.asi
SOURCES  := $(wildcard src/*.cpp)
OBJECTS  := $(patsubst src/%.cpp,build/%.o,$(SOURCES))

CXXFLAGS := -m32 -std=c++20 -O2 -Wall -Wextra -fno-strict-aliasing \
            -fno-exceptions -fno-rtti -DNOMINMAX -DWIN32_LEAN_AND_MEAN
LDFLAGS  := -m32 -shared -static -static-libgcc -static-libstdc++ \
            -Wl,--exclude-all-symbols -Wl,-u,___mingw_SEH_error_handler -ladvapi32

.PHONY: all clean install

all: $(TARGET)

build:
	@mkdir -p build

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

clean:
	@rm -rf build

install: $(TARGET)
ifndef GAME
	$(error Set GAME to your Most Wanted folder, e.g. mingw32-make install GAME="C:/Games/NFSMW")
endif
	@mkdir -p "$(GAME)/scripts"
	cp $(TARGET) "$(GAME)/scripts/"
	cp NFSMWFairBusts.ini "$(GAME)/scripts/"
	@echo "Installed to $(GAME)/scripts"
