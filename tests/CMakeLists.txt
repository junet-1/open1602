# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(o1602_tests
    test_main.cpp
    unit/test_assets.cpp
    unit/test_cod_parser.cpp
    data/test_original_data.cpp
)
target_link_libraries(o1602_tests PRIVATE o1602::assets)
o1602_set_warnings(o1602_tests)

add_test(NAME unit COMMAND o1602_tests)
