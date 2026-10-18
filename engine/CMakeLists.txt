# SPDX-License-Identifier: GPL-3.0-or-later
#
# Engine modules. Dependency direction (lower may never depend on higher):
#
#   core  <-  assets  <-  sim  <-  net
#                           ^
#   platform  <-  render / audio / video / ui  <-  app
#
# 'sim' must never depend on platform, render, audio or ui: the simulation has to stay
# deterministic and runnable headless (tests, dedicated server, replays).

add_subdirectory(core)
add_subdirectory(assets)
add_subdirectory(sim)
add_subdirectory(net)
add_subdirectory(render)
add_subdirectory(audio)
add_subdirectory(video)
add_subdirectory(ui)
add_subdirectory(app/menus)

if(O1602_BUILD_GAME)
    add_subdirectory(platform)
    add_subdirectory(render/sdl)
    add_subdirectory(app)
endif()

# Modules below are planned (see docs/00-plan.md) and get added as they are implemented:
#   script
