# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(o1602_tests
    test_main.cpp
    unit/test_assets.cpp
    unit/test_cod_parser.cpp
    unit/test_island_render.cpp
    unit/test_world_render.cpp
    unit/test_players_ships.cpp
    unit/test_ships.cpp
    unit/test_diplomacy.cpp
    unit/test_military.cpp
    unit/test_naval.cpp
    unit/test_figures.cpp
    unit/test_production.cpp
    unit/test_logistics.cpp
    unit/test_raw_producers.cpp
    unit/test_consumption.cpp
    unit/test_random.cpp
    unit/test_houses.cpp
    unit/test_island_events.cpp
    unit/test_construction.cpp
    unit/test_drag_path.cpp
    unit/test_deer.cpp
    unit/test_disasters.cpp
    unit/test_volcanoes.cpp
    unit/test_ambient.cpp
    unit/test_units.cpp
    data/test_original_data.cpp
    data/test_new_game.cpp
    data/test_ships_data.cpp
    data/test_military_data.cpp
)
target_link_libraries(o1602_tests PRIVATE o1602::assets o1602::sim o1602::render)
o1602_set_warnings(o1602_tests)

add_test(NAME unit COMMAND o1602_tests)
