# NFSMW Fair Busts - 32-bit ASI plugin
#
#   mingw32-make                     build build/NFSMWFairBusts.asi
#   mingw32-make clean
#   mingw32-make install GAME="C:/path/to/NFSMW"
#
# Requires a 32-bit MinGW-w64 g++ (i686-w64-mingw32). The plugin uses no C++
# runtime, so nothing beyond libgcc is linked. For MSVC see README.md.

CXX      ?= g++
TARGET   := build/NFSMWFairBusts.asi
SOURCES  := $(wildcard src/*.cpp)
OBJECTS  := $(patsubst src/%.cpp,build/%.o,$(SOURCES))

CXXFLAGS := -m32 -std=c++17 -O2 -Wall -Wextra -fno-strict-aliasing \
            -fno-exceptions -fno-rtti
LDFLAGS  := -m32 -shared -static -static-libgcc -Wl,--exclude-all-symbols -ladvapi32

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
