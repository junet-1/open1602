# engine/app

Program entry point and wiring of all modules. Currently the map viewer `open1602`
(milestone M2): load a scenario, display the world map with SDL3, scrolling, zoom, rotation.
Game states (main menu, game, editor) and configuration come later.

Only with `-DO1602_BUILD_GAME=ON`. Controls: [docs/setup-arch.md](../../docs/setup-arch.md#map-viewer-open1602-sdl3).
