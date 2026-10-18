# SPDX-License-Identifier: GPL-3.0-or-later
# Game executable. For now the map viewer (milestone M2).

add_executable(open1602
    src/main.cpp
    src/build_tool.cpp
    src/game_sound.cpp
    src/hud.cpp
    src/map_viewer.cpp
    src/video_player.cpp
)
target_link_libraries(open1602 PRIVATE o1602::sim o1602::render_sdl o1602::platform o1602::audio o1602::ui o1602::video)
o1602_set_warnings(open1602)

if(WIN32)
    set_target_properties(open1602 PROPERTIES WIN32_EXECUTABLE ON)
endif()

install(TARGETS open1602 RUNTIME DESTINATION bin)
