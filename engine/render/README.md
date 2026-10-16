# engine/render

Isometrische Darstellung nach dem Originalverfahren ([docs/re/rendering.md](../../docs/re/rendering.md)).

| Header | Inhalt |
|---|---|
| `island_renderer.hpp` | Zeichenliste (Sprite, Position, Maleralgorithmus) + Software-Rasterizer |

Geplant: GPU-Backend über SDL3 (Sprite-Atlanten, Batching), stufenloser Zoom, beliebige Auflösungen.
Liest den Simulationszustand nur.
