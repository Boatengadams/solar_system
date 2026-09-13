CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
SOURCES := $(shell find src -name '*.cpp' -print)
CPPFLAGS := -Isrc -Ithird_party/nlohmann-json3-dev/usr/include

.PHONY: all run clean

ifeq ($(strip $(RAYLIB_LIBS)),)
$(error raylib is not installed; run: sudo apt install g++ cmake pkg-config libraylib-dev)
endif

all: planets

planets: $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(RAYLIB_CFLAGS) $(SOURCES) -o $@ $(RAYLIB_LIBS)

run: planets
	./planets

clean:
	rm -f planets
