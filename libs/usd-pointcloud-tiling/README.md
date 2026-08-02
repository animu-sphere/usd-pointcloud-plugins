# usdPointCloudTiling

`usdPointCloudTiling` defines format-independent spatial partitioning
contracts for point clouds. It assigns source-coordinate points to stable
fixed-grid tile IDs without depending on LAS, LAZ, OpenUSD, or filesystem
spooling.

CMake target `usdpointcloud::tiling` and namespace `usdpointcloud`.

The initial contract contains `TileGridConfig`, `TileRouter`, and
`FixedGridTileRouter`. Tile indices use `floor(sourceCoordinate / tileSize)`,
so points on a boundary enter the tile to their positive side and negative
coordinates remain deterministic. The router uses source X/Y and ignores Z.

Spooling, bounded-memory buffering, tile manifests, and payload authoring are
separate follow-up contracts described in `docs/roadmap/streaming-and-tiling.md`.
