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

`SpoolSchema`, `TileSpoolWriter`, and `TileSpoolReader` define a versioned,
source-and-stage-coordinate binary spool. Records are written in append order,
the footer commits the point count, and readers use the fixed record layout to
reject missing or truncated footers. Attribute values are serialized using the
scalar types declared by the schema. The writer buffers records up to its
configured byte threshold before flushing; callers own the working directory
and can remove it with `RemoveSpoolDirectory`.

Tile routing, bounded-memory buffering, tile manifests, and payload authoring
remain separate follow-up contracts described in
`docs/roadmap/streaming-and-tiling.md`.
