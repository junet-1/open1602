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

**Open:** palette animation (water, fire?) – check whether the game rotates entries at runtime
(`IDirectDrawPalette::SetEntries`).
