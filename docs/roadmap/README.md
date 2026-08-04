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

The current major capability is **bounded-memory streaming into spatially
tiled, payload-backed `usdLod` assets**. Tiled reads now spool by source tile
and reconstruct one tile at a time. The release train is now split into two
deliberate steps:

- **`v0.2.x` — stabilization:** keep the existing LAS and LAZ behavior and
  public contracts stable while closing real-world RSS, payload working-set,
  compatibility, documentation, and operational reliability gaps.
- **`v0.3.0` — COPC:** add read-only COPC support for local files, reusing the
  shared point, streaming, tiling, diagnostics, and `usdLod` contracts. COPC
  hierarchy and byte ranges are mapped onto the shared representation; COPC
  writing and remote HTTP range access are outside the initial release.

The full plan — the `PointStream` interface, the spill-backed fixed-grid
tiling design, spool and payload contracts, the file-format argument surface,
the pull-request sequence, testing requirements, and the definition of done —
is in [streaming and tiling](streaming-and-tiling.md).

COPC follows LAS and LAZ because it validates native spatial hierarchy and
partial loading using the infrastructure completed for `v0.2.x`. E57 follows
COPC and reuses the same streaming, tiling, diagnostics, and authoring
infrastructure. GeoTIFF, DEM, and COG remain a later family of work because
raster and terrain data require different storage and authoring contracts from
point clouds.

## Release tracks

### `v0.2.x` — existing implementation stabilization

The `v0.2.x` line does not introduce another point-cloud format. It stabilizes
the LAS and LAZ implementation shipped in `v0.2.0`:

- measure real-dataset processing time, peak RSS, spool usage, and payload
  working set;
- fix reliability issues in long-running, cancelled, failed, and
  interrupted tiled reads, including temporary-file cleanup and rollback;
- keep LAS and LAZ output, metadata, diagnostics, normalized arguments, and
  tile/LOD contracts backward compatible;
- complete compatibility, installation, licensing, and large-data usage
  documentation; and
- add regression coverage for every behavioral fix without widening the
  public format surface unnecessarily.

`v0.2.x` is complete when the existing LAS/LAZ path has published real-world
measurements, no known resource-leak or cleanup issue remains in the tested
failure paths, and the release documentation matches the shipped behavior.

### `v0.3.0` — COPC read support

The first COPC release is deliberately read-first and local-first:

- add a format-specific COPC reader, preferably as `libs/usd-copc`, with a
  thin `plugins/geospatial-copc` adapter;
- inspect and validate COPC information and hierarchy metadata before point
  decoding;
- read only the hierarchy nodes and point-data byte ranges required for the
  requested operation;
- reuse the existing point attributes, Extra Bytes, metadata-only,
  `PointStream`, memory-budget, typed-diagnostics, tiling, and `usdLod`
  contracts;
- preserve the COPC hierarchy and resolution information when authoring the
  shared USD tile/LOD representation; and
- verify LAS, LAZ, and COPC equivalence for bounds, counts, coordinates,
  attributes, metadata, LOD, and diagnostics on equivalent fixtures.

COPC writing, conversion to COPC, HTTP byte-range sources, network caching,
and COPC-specific public USD schemas are deferred until the local read path
and its resource behavior are proven. COPC-specific parsing stays out of
`usd-laz`; only genuinely shared LAZ decoding or byte-range primitives may be
factored into a common implementation.

## Phases

| Phase | Scope | Status | Notes |
| --- | --- | --- | --- |
| 0 | Technical validation of the FileFormat Plugin and dependencies | In progress | PoC, adapter split, and precision path are done; large-data timing and memory measurements are outstanding |
| 1 | `usdGeoCore`, `usdPointCloudAuthoring`, and point-cloud contracts | In progress | Shipped in v0.1.0 except `usdGeoCache` |
| 2 | Direct LAS loading and `UsdGeomPoints` | Complete | Point formats 0-10, LAS 1.4 attributes, waveform metadata, GeoTIFF keys, and scalar and vector Extra Bytes all land on `main` |
| 3 | LAZ, attribute selection, and USDC caching | In progress | LAZ chunk decoding and normalized attribute selection shipped; the USDC cache is not started |
| 4a | Shared tile and LOD contracts and `usdLod` authoring | Complete | `usdLod` authoring, compact LOD profiles, deterministic sampling, per-tile roots, and payload-backed tile assets are available through the authoring library |
| 4b | Bounded-memory streaming and spatial tiling through the plugins | Stabilization in `v0.2.x` | `PointStream`, spill-backed routing, payload authoring, and the `tile` argument are connected; real-dataset and payload working-set measurements plus operational hardening remain |
| 4c | COPC read support | Planned for `v0.3.0` | Local read-only support first; reuse the `v0.2.x` contracts and preserve the native hierarchy; no writer or remote range source initially |
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
| W2 | LAS attribute coverage, GeoTIFF CRS, Extra Bytes | 2 | Complete for scalar and vector Extra Bytes, including name normalization |
| W3 | Point formats 4, 5, 9, 10 and the waveform contract | 2 | Complete |
| W4 | Chunked and range-based reader API, memory budget, filtering | 3 | Reader API and memory budget complete; bounds and classification filters open |
| W5 | Shared tile and LOD contracts, deterministic sampling, OpenUSD 26.08 `usdLod` authoring | 4a | Complete |
| W6 | `PointStream`, spill-backed spatial tiling, payload generation during file open, spatial tile arguments, and LAS/LAZ stabilization | 4b | `v0.2.x` stabilization in progress |
| W7 | COPC hierarchy, partial reads, and local COPC FileFormat integration | 4c | Planned for `v0.3.0` |
| W8 | USDC cache, remote byte-range sources, PLY, delimited text, E57, terrain rasters, and later formats | 3-7 | Deferred until the `v0.3.0` COPC boundary is stable |

W1 through W5 stabilized the shared point schema, the streaming reader API, and
the public LOD representation. W6 now stabilizes how much memory a tiled read
costs rather than what it produces. W6 is the entry gate for W7: COPC should
consume these contracts, not create a parallel streaming or authoring path.

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
