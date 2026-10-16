# SPDX-License-Identifier: GPL-3.0-or-later
# Deterministic simulation. Must not depend on platform, render, audio or ui (ADR 0004, 0005).

add_library(o1602_sim
    src/island_map.cpp
    src/player.cpp
    src/production_cycle.cpp
    src/simulation.cpp
    src/world.cpp
)
add_library(o1602::sim ALIAS o1602_sim)

target_include_directories(o1602_sim PUBLIC include)
target_link_libraries(o1602_sim PUBLIC o1602::assets)
o1602_set_warnings(o1602_sim)
