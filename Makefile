# Makefile - Linux build for the C/C++ exam trainer.
# Windows and macOS use CMake (see CMakeLists.txt).
#
#   make          build the app (fetches and builds Raylib on first run)
#   make run      build and start the app
#   make test     build and run the unit tests
#   make clean    remove build output (keeps the Raylib checkout)

RAYLIB_TAG  := 6.2
RAYLIB_URL  := https://github.com/raysan5/raylib.git
RAYLIB_DIR  := third_party/raylib
RAYLIB_SRC  := $(RAYLIB_DIR)/src
RAYLIB_LIB  := $(RAYLIB_SRC)/libraylib.a

CXX      ?= g++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic -MMD -MP -Isrc
LDLIBS    = -lm -lpthread -ldl -lrt -lGL -lX11

BUILD    := build
APP      := $(BUILD)/learn-c-cpp
TESTBIN  := $(BUILD)/unit-tests

SRC_ALL  := $(shell find src -name '*.cpp')
SRC_APP  := $(SRC_ALL)
OBJ_APP  := $(patsubst %.cpp,$(BUILD)/obj/%.o,$(SRC_APP))

# Raylib-free sources the unit tests link against.
SRC_CORE := $(filter src/content/% src/logic/%,$(SRC_ALL))
SRC_TEST := $(wildcard tests/*.cpp)
OBJ_TEST := $(patsubst %.cpp,$(BUILD)/obj/%.o,$(SRC_CORE) $(SRC_TEST))

.PHONY: all run test export-content clean distclean raylib

all: $(APP)

$(APP): $(OBJ_APP) $(RAYLIB_LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ_APP) $(RAYLIB_LIB) $(LDLIBS) -o $@

$(BUILD)/obj/%.o: %.cpp | $(RAYLIB_SRC)/raylib.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(RAYLIB_SRC) -c $< -o $@

# Fetch the pinned Raylib release if it is not there yet.
$(RAYLIB_SRC)/raylib.h:
	git clone --depth 1 --branch $(RAYLIB_TAG) $(RAYLIB_URL) $(RAYLIB_DIR)

raylib: $(RAYLIB_LIB)

$(RAYLIB_LIB): | $(RAYLIB_SRC)/raylib.h
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP

run: $(APP)
	./$(APP)

test: $(TESTBIN)
	./$(TESTBIN)

$(TESTBIN): $(OBJ_TEST)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ_TEST) -o $@

# Content verification helper: make export-content OUT=/some/dir [RANDOM=n] dumps every runnable snippet
# (and n random exams built from the reference solutions).
EXPORT := $(BUILD)/export-content
RANDOM_EXAMS ?= 0
OBJ_EXPORT := $(patsubst %.cpp,$(BUILD)/obj/%.o,$(SRC_CORE) tools/export_content.cpp)

$(EXPORT): $(OBJ_EXPORT)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ_EXPORT) -o $@

export-content: $(EXPORT)
	./$(EXPORT) content $(OUT) $(RANDOM_EXAMS)

clean:
	rm -rf $(BUILD)

distclean: clean
	rm -rf third_party

-include $(OBJ_APP:.o=.d) $(OBJ_TEST:.o=.d) $(OBJ_EXPORT:.o=.d)
