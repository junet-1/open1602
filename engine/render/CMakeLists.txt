# SPDX-License-Identifier: GPL-3.0-or-later
# Rendering. The software path (island_renderer) has no platform dependency and is used by tools
# and tests; the SDL3/GPU backend will be added as a separate target.

add_library(o1602_render
    src/animation_clock.cpp
    src/camera.cpp
    src/island_renderer.cpp
    src/sprite_atlas.cpp
    src/world_renderer.cpp
)
add_library(o1602::render ALIAS o1602_render)

target_include_directories(o1602_render PUBLIC include)
target_link_libraries(o1602_render PUBLIC o1602::sim o1602::assets)
o1602_set_warnings(o1602_render)
