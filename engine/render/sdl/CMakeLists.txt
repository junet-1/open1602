# SPDX-License-Identifier: GPL-3.0-or-later
# SDL3 backend of the renderer. Separate target so tools and tests stay free of SDL.

add_library(o1602_render_sdl
    src/sdl_sprite_renderer.cpp
)
add_library(o1602::render_sdl ALIAS o1602_render_sdl)

target_include_directories(o1602_render_sdl PUBLIC include)
target_link_libraries(o1602_render_sdl PUBLIC o1602::render o1602::platform)
o1602_set_warnings(o1602_render_sdl)
