cmake_minimum_required(VERSION 3.16)
project(earthbound_switch_widescreen_layout C)
set(CMAKE_C_STANDARD 11)
enable_testing()

add_library(eb_switch_widescreen STATIC layout.c)
target_include_directories(eb_switch_widescreen PUBLIC ${CMAKE_CURRENT_LIST_DIR})
add_executable(eb_switch_widescreen_test test_layout.c)
target_link_libraries(eb_switch_widescreen_test PRIVATE eb_switch_widescreen)
add_test(NAME widescreen_layout COMMAND eb_switch_widescreen_test)

add_library(eb_switch_input_mapping STATIC ../input_mapping.c)
target_include_directories(eb_switch_input_mapping PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/..
    ${CMAKE_CURRENT_LIST_DIR}/../../../src)
add_executable(eb_switch_input_mapping_test ../test_input_mapping.c)
target_link_libraries(eb_switch_input_mapping_test PRIVATE eb_switch_input_mapping)
add_test(NAME switch_button_mapping COMMAND eb_switch_input_mapping_test)
