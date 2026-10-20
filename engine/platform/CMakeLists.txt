# SPDX-License-Identifier: GPL-3.0-or-later
# Platform layer (SDL3): window, renderer, input, timing. Only built with O1602_BUILD_GAME.

find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
# SDL3Config marks its headers as non-system; our strict warning flags would then apply to them.
if(TARGET SDL3::Headers)
    set_property(TARGET SDL3::Headers PROPERTY SYSTEM 1)
endif()

add_library(o1602_platform
    src/audio_output.cpp
    src/mouse_cursor.cpp
    src/window.cpp
)
add_library(o1602::platform ALIAS o1602_platform)

target_include_directories(o1602_platform PUBLIC include)
target_link_libraries(o1602_platform PUBLIC o1602::core SDL3::SDL3)
o1602_set_warnings(o1602_platform)
