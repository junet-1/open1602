# SPDX-License-Identifier: GPL-3.0-or-later
# Loaders for the original Anno 1602 data formats. No platform or rendering dependencies.

add_library(o1602_assets
    src/building_table.cpp
    src/chunk_file.cpp
    src/chunk_writer.cpp
    src/cod.cpp
    src/cod_parser.cpp
    src/cod_predefined.cpp
    src/cursor_resource.cpp
    src/figure_table.cpp
    src/gad.cpp
    src/high_scores.cpp
    src/game_directory.cpp
    src/island.cpp
    src/palette.cpp
    src/savegame.cpp
    src/scenario.cpp
    src/sprite_sheet.cpp
    src/text_table.cpp
)
add_library(o1602::assets ALIAS o1602_assets)

target_include_directories(o1602_assets PUBLIC include)
target_link_libraries(o1602_assets PUBLIC o1602::core)
o1602_set_warnings(o1602_assets)
