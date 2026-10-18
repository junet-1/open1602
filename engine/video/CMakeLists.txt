# SPDX-License-Identifier: GPL-3.0-or-later
# Videos: the Smacker decoder. No platform dependency (the frontend shows the frames and hands the
# sound to the mixer), so tests run it headless.

add_library(o1602_video
    src/smacker.cpp
)
add_library(o1602::video ALIAS o1602_video)

target_include_directories(o1602_video PUBLIC include)
target_link_libraries(o1602_video PUBLIC o1602::audio o1602::core)
o1602_set_warnings(o1602_video)
