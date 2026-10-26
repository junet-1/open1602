# COL – palette

`TOOLGFX/STADTFLD.COL` (1044 bytes) = one chunk `COL` with 1024 bytes of payload:

```c
struct ColEntry { uint8_t r, g, b, unused; };   // unused is always 0
ColEntry palette[256];
```

Entry 0 is black. The same palette applies to all sprites from `GFX/`, `MGFX/`, `SGFX/`
and `TOOLGFX/` (checked visually: terrain, buildings, ships, carriers, main menu).
The game loads it in `Game_Init` (`0x00494870`, string `stadtfld.col` @ `0x0049C4B0`).

Transparency is not represented by a palette entry but by the RLE encoding of the sprites
(see [bsh.md](bsh.md)).

**No palette animation.** The game's only call of `IDirectDrawPalette::SetEntries` is in
`Palette_Apply` (`0x00491B30`): full screen it sets all 256 entries, in a window entries 10–245 (the
rest are Windows' system colours). It runs only when the display mode is set (`0x00492840`) and when the
surfaces are restored (`0x00492410`), never per frame. Moving water, fire and smoke are sprite animations.
