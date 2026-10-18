# SPDX-License-Identifier: GPL-3.0-or-later
# The interface without platform dependency: fonts and text, gadget layouts at work, regions.

add_library(o1602_ui
    src/build_menu.cpp
    src/font.cpp
    src/panel.cpp
    src/text_format.cpp
)
add_library(o1602::ui ALIAS o1602_ui)

target_include_directories(o1602_ui PUBLIC include)
target_link_libraries(o1602_ui PUBLIC o1602::assets o1602::core)
o1602_set_warnings(o1602_ui)
