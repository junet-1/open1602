# engine/render

Isometric rendering following the original's method ([docs/re/rendering.md](../../docs/re/rendering.md)).

| Header | Contents |
|---|---|
| `island_renderer.hpp` | Draw list (sprite, position, painter's algorithm) + software rasteriser |

Planned: GPU backend via SDL3 (sprite atlases, batching), continuous zoom, arbitrary resolutions.
Only reads the simulation state.
