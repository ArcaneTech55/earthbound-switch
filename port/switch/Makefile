# Run from port/switch; the CMake project compiles every relevant src/
# core file, generates embedded assets, and links the SDL2/libnx port.
# Prerequisites: devkitPro switch-dev, switch-sdl2, Python ebtools, extracted
# assets from your own EarthBound ROM (see ../../README.md).
.DEFAULT_GOAL := all

DEVKITPRO ?= /opt/devkitpro
TOOLCHAIN ?= $(DEVKITPRO)/cmake/Switch.cmake
BUILD_DIR ?= build
CMAKE ?= cmake
NINJA ?= ninja
CONFIG ?= Release
WIDESCREEN ?= ON

.PHONY: all check configure build clean rebuild tests

all: build

check:
	@test -f "$(TOOLCHAIN)" || { echo "Missing Switch toolchain: $(TOOLCHAIN)"; exit 1; }
	@test -x "$(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc" || { echo "Missing devkitA64"; exit 1; }
	@command -v ebtools >/dev/null || { echo "Missing host ebtools"; exit 1; }
	@test -f "../../asm/bin/assets.manifest" || { echo "Extract your own ROM assets first: make -C ../.. extract"; exit 1; }
	@command -v "$(NINJA)" >/dev/null || { echo "Missing Ninja"; exit 1; }

configure: check
	DEVKITPRO="$(DEVKITPRO)" "$(CMAKE)" -S . -B "$(BUILD_DIR)" -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE="$(TOOLCHAIN)" \
		-DCMAKE_BUILD_TYPE="$(CONFIG)" \
		-DEB_SWITCH_TRUE_WIDESCREEN="$(WIDESCREEN)"

build: configure
	"$(CMAKE)" --build "$(BUILD_DIR)" --parallel
	@test -s "$(BUILD_DIR)/EarthboundSwitch.nro"
	@echo "Built $(BUILD_DIR)/EarthboundSwitch.nro"

clean:
	"$(CMAKE)" -E rm -rf "$(BUILD_DIR)"

rebuild: clean all

tests:
	"$(CMAKE)" -S widescreen -B "$(BUILD_DIR)/host-tests"
	"$(CMAKE)" --build "$(BUILD_DIR)/host-tests"
	ctest --test-dir "$(BUILD_DIR)/host-tests" --output-on-failure
