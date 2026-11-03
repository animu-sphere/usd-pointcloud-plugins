# pointcloud-ply

`pointcloud-ply` is the OpenUSD FileFormat Plugin adapter for scalar PLY point
clouds. It delegates decoding to `usdPly`, uses the shared point authoring
contracts, and requires an explicit CRS because PLY has no embedded CRS model.

## Supported reads

- PLY 1.0 ASCII, binary little-endian, and binary big-endian scalar vertex data.
- `x`, `y`, `z`, RGB, intensity, classification, and generic scalar properties.
- Bounded source streaming with chunk, memory, range, bounds, classification,
  and cancellation controls.
- Shared non-tiled authoring, deterministic cache lookup through
  `USDGEO_CACHE_ROOT`, and payload-backed fixed-grid tiled authoring.

The `epsg` file-format argument is required. `linearUnit`, `sourceUpAxis`, and
`stageUpAxis` are optional. Tiled reads use `tileSize`, `tileMemoryLimit`, and
`payloadDirectory`, below which each layer publishes its payloads in a
directory of its own. See
[payload ownership](../../docs/architecture/FILE_FORMAT_ARGUMENTS.md#generated-payload-ownership).

A tiled read that cannot author reports `PLY007` with the reason, or `PLY008`
when the read was stopped by its own working storage (tile spool, spool
workspace, file-handle budget, or payload publication) rather than by the
source. See the [diagnostics contract](../../docs/architecture/DIAGNOSTICS.md#working-storage-codes).

Faces, mesh authoring, PLY writing, and metadata-only reads are out of scope.
See the [capability matrix](../../docs/reference/CAPABILITY_MATRIX.md) and
[file-format argument contract](../../docs/architecture/FILE_FORMAT_ARGUMENTS.md)
for the complete public behavior.
