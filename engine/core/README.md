# engine/core

Basic building blocks without game specifics: `Result<T>`/`Error`, bounds-checked `ByteReader`,
file access including case-insensitive path resolution, Windows-1252 → UTF-8,
PNG output. Later: logging, fixed-point arithmetic, deterministic RNG, memory pools.
