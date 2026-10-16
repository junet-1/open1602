# SPDX-License-Identifier: GPL-3.0-or-later
# Loaders for the original Anno 1602 data formats. No platform or rendering dependencies.

add_library(o1602_assets
    src/chunk_file.cpp
    src/cod.cpp
    src/game_directory.cpp
    src/island.cpp
    src/palette.cpp
    src/sprite_sheet.cpp
)
add_library(o1602::assets ALIAS o1602_assets)

target_include_directories(o1602_assets PUBLIC include)
target_link_libraries(o1602_assets PUBLIC o1602::core)
o1602_set_warnings(o1602_assets)
