# engine/assets

Loaders for the original formats (specification in [docs/formats](../../docs/formats/README.md)):

| Header | Format |
|---|---|
| `chunk_file.hpp` | Chunk container (basis of almost all binary files) |
| `cod.hpp` | Decrypting `.cod` scripts |
| `cod_parser.hpp` | `.cod` interpreter (symbols, expressions, objects, `ObjFill`) |
| `cod_predefined.hpp` | constants predefined by the exe (generated) |
| `palette.hpp` | `.COL` palette |
| `sprite_sheet.hpp` | `.BSH`/`.ZEI` sprites (RLE) |
| `island.hpp` | Islands from `.SCP`/`.SZS`/`.SZM`/`.GAM`, attribute bits of the tiles |
| `building_table.hpp` | typed building/terrain types from `haeuser.cod`, lookup by id and sprite |
| `game_directory.hpp` | Access to an original installation |

No dependency on platform or graphics; every function returns `core::Result` and does not crash on
faulty data.
