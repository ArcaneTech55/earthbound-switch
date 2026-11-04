cmake_minimum_required(VERSION 3.20)
project(EarthboundSwitch LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The devkitPro toolchain must be selected on the first CMake configure:
# -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Set DEVKITPRO and install devkitPro switch-dev.")
endif()
set(DEVKITPRO "$ENV{DEVKITPRO}")
if(NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Switch build requires the devkitPro Switch toolchain. Run make in port/switch.")
endif()

# Add Switch portlibs to the search path BEFORE find_package.
list(PREPEND CMAKE_PREFIX_PATH "${DEVKITPRO}/portlibs/switch")
find_package(SDL2 REQUIRED)

# Widescreen dimensions are **engine render dimensions**, not window dimensions.
# Never change SNES_WIDTH/SNES_HEIGHT (hardware constants) in src/core/types.h.
option(EB_SWITCH_TRUE_WIDESCREEN "Render game at 400x224 rather than 256x224" ON)
if(EB_SWITCH_TRUE_WIDESCREEN)
    set(EB_VIEWPORT_WIDTH 400 CACHE STRING "Engine framebuffer stride (pixels)" FORCE)
else()
    set(EB_VIEWPORT_WIDTH 256 CACHE STRING "Engine framebuffer stride (pixels)" FORCE)
endif()
set(EB_VIEWPORT_HEIGHT 224 CACHE STRING "Engine framebuffer height" FORCE)
set(EB_ENABLE_AUDIO ON CACHE BOOL "Build the SPC700/DSP audio core" FORCE)
set(EB_SMOOTH_CAMERA OFF CACHE BOOL "Disable SDL desktop-specific camera hooks" FORCE)
set(EB_WERROR OFF CACHE BOOL "Allow upstream warnings while porting" FORCE)

# src/CMakeLists.txt owns core source discovery, vendor libs, generated asset
# tables, and music/item headers. Plain recursive source compilation here
# would omit required asset generation and break the game.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../src"
                 "${CMAKE_CURRENT_BINARY_DIR}/game_lib")

# The embedded savestate stub is compiled as part of earthbound_game.
target_compile_definitions(earthbound_game PUBLIC EB_EMBEDDED=1)
target_compile_definitions(earthbound_game PRIVATE EB_SWITCH_SCENE_ART=1)
target_include_directories(earthbound_game PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/widescreen")

add_executable(earthbound_switch
    main.c
    input_mapping.c
    widescreen/layout.c
    widescreen/scaling.c
    widescreen/scene_art.c
    platform/switch_sdl2_input.c
    platform/switch_sdl2_video.c
    platform/switch_debug.c
    ../unix/platform/sdl2_timer.c
    ../unix/platform/sdl2_audio.c
    ../unix/platform/sdl2_save.c
)

target_include_directories(earthbound_switch PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    "${DEVKITPRO}/libnx/include"
    "${DEVKITPRO}/portlibs/switch/include"
)

target_compile_definitions(earthbound_switch PRIVATE __SWITCH__=1)
target_compile_options(earthbound_switch PRIVATE -Wall -Wextra)
target_link_directories(earthbound_switch PRIVATE
    "${DEVKITPRO}/libnx/lib"
    "${DEVKITPRO}/portlibs/switch/lib"
)

# SDL2 Switch portlibs may provide SDL2::SDL2 or only SDL2_LIBRARIES.
if(TARGET SDL2::SDL2)
    target_link_libraries(earthbound_switch PRIVATE earthbound_game SDL2::SDL2 nx m)
else()
    if(SDL2_INCLUDE_DIRS)
        target_include_directories(earthbound_switch PRIVATE ${SDL2_INCLUDE_DIRS})
    endif()
    if(NOT SDL2_LIBRARIES)
        message(FATAL_ERROR "SDL2 found, but it exposes no libraries for linking.")
    endif()
    target_link_libraries(earthbound_switch PRIVATE
        earthbound_game ${SDL2_LIBRARIES} nx m)
endif()

# The devkitPro toolchain typically supplies switch.specs automatically.
# Avoid adding it twice; verify toolchain ABI/library flags in its own config.
set_target_properties(earthbound_switch PROPERTIES
    OUTPUT_NAME "EarthboundSwitch"
    SUFFIX ".elf"
)

# devkitPro's official CMake integration provides nx_generate_nacp()
# and nx_create_nro(). add_nro_target() is NOT a standard devkitPro macro,
# so expose this small project-local wrapper with an unambiguous contract.
if(NOT COMMAND nx_generate_nacp OR NOT COMMAND nx_create_nro)
    message(FATAL_ERROR
        "devkitPro Switch.cmake must provide nx_generate_nacp and nx_create_nro.")
endif()

function(add_nro_target target)
    nx_generate_nacp(EarthboundSwitch.nacp
        NAME "EarthBound Switch"
        AUTHOR "EarthBound Switch Homebrew"
        VERSION "0.1.0"
    )
    nx_create_nro(${target}
        NACP EarthboundSwitch.nacp
    )
endfunction()

add_nro_target(earthbound_switch)
