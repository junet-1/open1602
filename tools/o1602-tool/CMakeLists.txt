# SPDX-License-Identifier: GPL-3.0-or-later
# o1602-tool: command line utility to inspect and convert original game data.

add_executable(o1602-tool main.cpp)
target_link_libraries(o1602-tool PRIVATE o1602::assets)
o1602_set_warnings(o1602-tool)
