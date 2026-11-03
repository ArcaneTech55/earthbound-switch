cmake_minimum_required(VERSION 3.16)
project(earthbound_switch C)
set(CMAKE_C_STANDARD 11)

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is required: install devkitPro switch-dev first")
endif()
set(DEVKITPRO "$ENV{DEVKITPRO}")
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|AArch64")
    message(WARNING "Configure this project with the devkitPro Switch toolchain, not the host compiler")
endif()

# First deliver a faithful, original-viewport Switch port, then expand
# scene rendering. Merely setting viewport width to 400 is not true 16:9.
set(EB_VIEWPORT_WIDTH 256 CACHE STRING "Initial renderer width" FORCE)
set(EB_VIEWPORT_HEIGHT 224 CACHE STRING "Initial renderer height" FORCE)
set(EB_ENABLE_AUDIO ON CACHE BOOL "Keep SPC700 audio engine" FORCE)
set(EB_EMBEDDED ON CACHE BOOL "Use safe no-op savestate backend" FORCE)
# The library (not just the executable) compiles the savestate stub.
# Its source checks EB_EMBEDDED at compile time.


add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../src
                 ${CMAKE_CURRENT_BINARY_DIR}/game_lib)

target_compile_definitions(earthbound_game PUBLIC EB_EMBEDDED=1)

add_executable(earthbound_switch
    main.c
    input_mapping.c
    platform/switch_sdl2_input.c
    platform/switch_sdl2_video.c
    platform/switch_debug.c
    ../unix/platform/sdl2_timer.c
    ../unix/platform/sdl2_audio.c
    ../unix/platform/sdl2_save.c
)
target_include_directories(earthbound_switch PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../../src
    "${DEVKITPRO}/portlibs/switch/include"
    "${DEVKITPRO}/libnx/include"
)
target_compile_definitions(earthbound_switch PRIVATE
    __SWITCH__=1 EB_ENABLE_AUDIO=1 EB_EMBEDDED=1)
target_link_directories(earthbound_switch PRIVATE
    "${DEVKITPRO}/portlibs/switch/lib" "${DEVKITPRO}/libnx/lib")
target_link_libraries(earthbound_switch PRIVATE
    earthbound_game SDL2 nx m)
target_link_options(earthbound_switch PRIVATE
    "-specs=${DEVKITPRO}/libnx/switch.specs")
set_target_properties(earthbound_switch PROPERTIES SUFFIX ".elf")

find_program(ELF2NRO elf2nro HINTS "${DEVKITPRO}/tools/bin")
if(NOT ELF2NRO)
    message(FATAL_ERROR "elf2nro is missing from devkitPro tools")
endif()
add_custom_command(TARGET earthbound_switch POST_BUILD
    COMMAND "${ELF2NRO}" "$<TARGET_FILE:earthbound_switch>"
            "${CMAKE_CURRENT_BINARY_DIR}/earthbound_switch.nro"
    COMMENT "Packing homebrew NRO")
