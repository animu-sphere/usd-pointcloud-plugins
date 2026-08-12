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

`PointBudgetConfig` and `BuildPointBudgetPlan` provide deterministic adaptive
planning over source X/Y positions. The planner preserves the fixed-grid
configuration separately, honors minimum and maximum points per tile and a
maximum depth, and reports point count, tile count, minimum, maximum, and
average leaf density, split count, and reached depth in `PointBudgetPlan`.
When the source distribution cannot satisfy the requested budget, the planner
returns a failure and emits a typed diagnostic instead of silently relaxing the
limit.

`SpoolSchema`, `TileSpoolWriter`, and `TileSpoolReader` define a versioned,
source-and-stage-coordinate binary spool. Records are written in append order,
the footer commits the point count, and readers use the fixed record layout to
reject missing or truncated footers. Attribute values are serialized using the
scalar types declared by the schema. The writer buffers records up to its
configured byte threshold before flushing; streaming callers may combine
multiple writers under a total working-set budget. Callers own the working
directory and can remove it with `RemoveSpoolDirectory`.

Tile manifests remain a separate follow-up contract described in
`docs/roadmap/streaming-and-tiling.md`.
