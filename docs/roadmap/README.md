# usd-geo-plugins Roadmap

This directory breaks the [design policy](../design/DESIGN_POLICY.md) into
actionable milestones. The policy states the standing direction; this
directory states the order of work. What is already implemented is in
[implementation status](implementation-status.md) and, at the level of source
support, in [capability matrix](../reference/CAPABILITY_MATRIX.md).

## Principles

- Keep `usdGeoCore` limited to shared CRS, origin, unit, bounds, tile, and
  diagnostic primitives.
- Keep OpenUSD authoring in `usdPointCloudAuthoring`, separate from pure data
  models and readers.
- Keep spatial partitioning in `usdPointCloudTiling`, free of both format
  parsing and OpenUSD types.
- Put format-specific readers in independent `libs/` modules, and keep the
  `plugins/` bundles thin adapters over them.
- Complete a read-first LAS / LAZ vertical slice first.
- Keep source coordinates and USD stage-local coordinates explicitly separate.
- Keep spatial tiling separate from level-of-detail selection.
- Author LOD only through the OpenUSD 26.08 `usdLod` schemas, and leave
  selection to the consuming application.
- Produce small, testable deliverables at every stage.

## Immediate direction

The next major capability is **bounded-memory streaming into spatially tiled,
payload-backed `usdLod` assets**. Readers already decode in chunks, but the
plugins still accumulate the complete cloud before authoring, so peak memory
is proportional to the point count.

The full plan — the `PointStream` interface, the spill-backed fixed-grid
tiling design, spool and payload contracts, the file-format argument surface,
the pull-request sequence, testing requirements, and the definition of done —
is in [streaming and tiling](streaming-and-tiling.md).

Once that path is stable, COPC and E57 reuse the same streaming, tiling,
diagnostics, and authoring infrastructure. GeoTIFF, DEM, and COG remain a
later family of work because raster and terrain data require different storage
and authoring contracts from point clouds.

## Phases

| Phase | Scope | Status | Notes |
| --- | --- | --- | --- |
| 0 | Technical validation of the FileFormat Plugin and dependencies | In progress | PoC, adapter split, and precision path are done; large-data timing and memory measurements are outstanding |
| 1 | `usdGeoCore`, `usdPointCloudAuthoring`, and point-cloud contracts | In progress | Shipped in v0.1.0 except `usdGeoCache` |
| 2 | Direct LAS loading and `UsdGeomPoints` | Complete | Point formats 0-10, LAS 1.4 attributes, waveform metadata, GeoTIFF keys, and scalar Extra Bytes all land on `main` |
| 3 | LAZ, attribute selection, and USDC caching | In progress | LAZ chunk decoding and normalized attribute selection shipped; the USDC cache is not started |
| 4a | Shared tile and LOD contracts and `usdLod` authoring | Complete | `usdLod` authoring, compact LOD profiles, deterministic sampling, per-tile roots, and payload-backed tile assets are available through the authoring library |
| 4b | Bounded-memory streaming and spatial tiling through the plugins | Not started | `PointStream`, `usdPointCloudTiling`, spill-backed routing, and the `tile` argument; see [streaming and tiling](streaming-and-tiling.md) |
| 4c | COPC | Not started | Reuses the phase 4b streaming and tiling infrastructure |
| 5 | PLY and delimited text point clouds (XYZ, PTS, CSV) | Not started | Needs the generic attribute model and file-format arguments |
| 6 | E57 and multi-scan point clouds | Not started | Extends the point-cloud contracts to several scans per file |
| 7 | GeoTIFF, DEM, and COG terrain | Not started | First non-point-cloud domain |
| 8 | GeoJSON and FlatGeobuf vector data | Not started | |
| 9 | GeoPackage, Shapefile, and additional interoperability formats | Deferred | |

## Workstreams

The policy orders work by capability rather than by format. Each workstream
maps onto the phases above.

| Workstream | Scope | Phases | Status |
| --- | --- | --- | --- |
| W1 | Public specification alignment, typed diagnostics, endian-safe decoding | 0, 1 | Complete |
| W2 | LAS attribute coverage, GeoTIFF CRS, Extra Bytes | 2 | Complete for scalar Extra Bytes; vector types and name normalization open |
| W3 | Point formats 4, 5, 9, 10 and the waveform contract | 2 | Complete |
| W4 | Chunked and range-based reader API, memory budget, filtering | 3 | Reader API and memory budget complete; bounds and classification filters open |
| W5 | Shared tile and LOD contracts, deterministic sampling, OpenUSD 26.08 `usdLod` authoring | 4a | Complete |
| W6 | `PointStream`, spill-backed spatial tiling, payload generation during file open, spatial tile arguments | 4b | Not started |
| W7 | USDC cache, COPC, PLY, delimited text, E57, terrain rasters, remote byte-range sources | 3-7 | Not started |

W1 through W5 stabilized the shared point schema, the streaming reader API, and
the public LOD representation. W6 is the first workstream that changes how much
memory a read costs rather than what a read produces, and it is the
prerequisite for W7's COPC work.

## Documents

- [Streaming and tiling](streaming-and-tiling.md)
- [Implementation status](implementation-status.md)
- [File format support order](format-support-order.md)
- [Phase 0](phase-0-technical-validation.md)
- [Phase 1](phase-1-geo-core.md)

Related documents outside this directory:

- [Design policy](../design/DESIGN_POLICY.md)
- [Workspace contract](../architecture/WORKSPACE.md)
- [Capability matrix](../reference/CAPABILITY_MATRIX.md)
- [Tile and LOD contract](../architecture/LOD.md)
- [Plugin adapter contract](../architecture/PLUGIN_ADAPTER.md)
- [File-format argument contract](../architecture/FILE_FORMAT_ARGUMENTS.md)
- [Point reader architecture](../architecture/POINT_READER.md)
- [Diagnostics contract](../architecture/DIAGNOSTICS.md)
- [OpenUSD compatibility](../compatibility/OPENUSD.md)
- [Migration](../compatibility/MIGRATION.md)
- [Binary distribution and licensing](../guides/DISTRIBUTION.md)
- [Coordinate model decision](../adr/0001-coordinate-model.md)
- [LAZ codec decision](../adr/0002-laz-codec.md)
- [Dynamic file format decision](../adr/0003-dynamic-file-format.md)
