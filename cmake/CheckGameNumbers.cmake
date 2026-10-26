# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks that the building records of haeuser.cod (Id 20001..23999) are named in one place (MD-1):
# engine/assets/include/o1602/assets/game_data.hpp, so that a data loader for mods has one table to
# replace. A number of that range anywhere else in the engine's code (comments aside) fails the
# check, unless the file is one of the readers and writers of the original formats or the number is
# not a record (sample rates, prices, the volcano table) and is listed below.
#
# Usage: cmake -DENGINE_DIR=<repo>/engine -P CheckGameNumbers.cmake   (ctest runs it as "game_numbers")

cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ENGINE_DIR)
    message(FATAL_ERROR "CheckGameNumbers.cmake: pass -DENGINE_DIR=<repo>/engine")
endif()

# Files below engine/ that may name records: the table itself, the predefined constants of the
# .cod files, the readable names of the records for mods (docs/modding/values.md) and the build
# menu, whose layout is the original's table (to become data with MD-4).
set(skipped
    "assets/include/o1602/assets/game_data.hpp"
    "assets/src/cod_predefined.cpp"
    "assets/src/original_names.cpp"
    "ui/src/build_menu.cpp"
)
# <file below engine/>=<numbers of that file that are not records>.
set(allowed
    "assets/src/building_table.cpp=23500"         # the highest Id the original accepts
    "assets/include/o1602/assets/building_table.hpp=23500"  # the same, BuildingTable::kMaxId
    "audio/src/wav.cpp=20350 22385"               # IMA ADPCM step table
    "audio/include/o1602/audio/wav.hpp=22050"     # sample rate
    "sim/src/volcanoes.cpp=21000"                 # eruption times
    "sim/src/ai/ai_planner.cpp=20001"             # gold
)

file(GLOB_RECURSE files "${ENGINE_DIR}/*.cpp" "${ENGINE_DIR}/*.hpp")
set(violations "")
set(checked 0)
foreach(file IN LISTS files)
    file(RELATIVE_PATH rel "${ENGINE_DIR}" "${file}")
    if(rel IN_LIST skipped)
        continue()
    endif()
    set(ok "")
    foreach(entry IN LISTS allowed)
        if(entry MATCHES "^${rel}=(.*)$")
            string(REPLACE " " ";" ok "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    math(EXPR checked "${checked} + 1")
    # Read whole and split at the line ends only: semicolons, brackets and backslashes would
    # change how CMake splits the list, so they become commas (file(STRINGS) splits at semicolons).
    file(READ "${file}" text)
    if(NOT text MATCHES "2[0-3][0-9][0-9][0-9]")
        continue()
    endif()
    string(REGEX REPLACE "[];[\\\\]" "," text "${text}")
    string(REPLACE "\n" ";" lines "${text}")
    foreach(line IN LISTS lines)
        # Code only: the part before a line comment, and not a comment line of a block.
        string(FIND "${line}" "//" comment)
        if(comment GREATER_EQUAL 0)
            string(SUBSTRING "${line}" 0 ${comment} line)
        endif()
        if(line MATCHES "^[ \t]*\\*")
            continue()
        endif()
        # A number on its own (not part of a longer number, a hex literal or a name); 20000 is the
        # offset of island tiles and save files to the Id, not a record.
        string(REGEX MATCHALL "(^|[^0-9A-Za-z_'.])2[0-3][0-9][0-9][0-9]($|[^0-9'])" hits " ${line}")
        foreach(hit IN LISTS hits)
            string(REGEX MATCH "2[0-3][0-9][0-9][0-9]" number "${hit}")
            if(number STREQUAL "20000" OR number IN_LIST ok)
                continue()
            endif()
            string(STRIP "${line}" code)
            list(APPEND violations "engine/${rel}: ${number} in \"${code}\"")
        endforeach()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n  " text)
    message(FATAL_ERROR "Building records named outside assets/game_data.hpp (use assets::id):\n  ${text}")
endif()
message(STATUS "Building records ok (${checked} files)")
