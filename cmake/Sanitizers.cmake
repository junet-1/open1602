# SPDX-License-Identifier: GPL-3.0-or-later
# Sanitizer support. Enable with -DO1602_SANITIZE=address;undefined (GCC/Clang only).

set(O1602_SANITIZE "" CACHE STRING "Semicolon separated list of sanitizers (address, undefined, thread, leak)")

if(O1602_SANITIZE AND NOT MSVC)
    list(JOIN O1602_SANITIZE "," _o1602_san)
    add_compile_options(-fsanitize=${_o1602_san} -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=${_o1602_san})
    message(STATUS "Open1602: sanitizers enabled: ${_o1602_san}")
endif()
