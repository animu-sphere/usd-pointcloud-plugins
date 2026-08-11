# usd-pointcloud-plugins Roadmap

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

LAS, LAZ, local COPC, PLY, bounded-memory tiling, generated-USDC caching, and
the conversion tool validate the shared point-cloud architecture. The next
release sequence prioritizes infrastructure maturity over adding formats. The
detailed rationale, scope, tests, and performance indicators are in the
[infrastructure maturity roadmap](infrastructure-maturity.md).

| Release | Theme | Outcome |
| --- | --- | --- |
| `v0.4.0` | PLY read support | Validate shared contracts outside the LAS family; released |
| `v0.5.0` | Resolver-backed COPC random access | Remote-capable COPC without coupling `usdCopc` to HTTP; released |
| `v0.6.0` | Cache and source identity | Deterministic local and conservative remote generated-output reuse; released |
| `v0.7.0` | Adaptive tiling | Point-budget-aware payload density and memory use |
| Later | Format expansion | E57 and other point-cloud adapters after infrastructure maturity |

The core architecture remains format-independent:

```text
LAS  -> usdLas  ----\
LAZ  -> usdLaz  -----+--> PointStream / usdPointCloudCore
COPC -> usdCopc -----/              |
                                   v
                         optional tiling / LOD
                                   |
                                   v
                      usdPointCloudAuthoring
                                   |
                                   v
                              OpenUSD stage
```

### `v0.3.x` — documentation and contract stabilization

The implementation and release engineering shipped in v0.3.0 ahead of several
public documents. This release line removes that drift before more architecture
is added.

- Simplify the root README into a product and contributor entry point. It
  summarizes support and links to canonical detail rather than duplicating the
  capability, argument, cache, and release documents.
- Synchronize the [workspace contract](../architecture/WORKSPACE.md) with the
  shipped tiling, cache, COPC, conversion-tool, fixture, and CI state.
- Keep [implementation status](implementation-status.md) and the
  [capability matrix](../reference/CAPABILITY_MATRIX.md) consistent with the
  workspace contract.
- Keep ownership explicit: structure belongs to `WORKSPACE.md`, support to
  `CAPABILITY_MATRIX.md`, arguments to `FILE_FORMAT_ARGUMENTS.md`, tile and
  LOD behavior to `LOD.md`, release behavior to `releases/vX.Y.Z.md`, and
  completed/open implementation work to `implementation-status.md`.

Exit criteria: the README, workspace contract, implementation checklist, and
capability matrix contain no known contradictions about LAS, LAZ, COPC,
tiling, cache reuse, conversion, fixtures, or CI.

### `v0.4.0` — PLY read support

PLY is the first intentional validation of the shared contracts outside the
LAS family. It has no LAS header or point-format model, no LAZ compression, no
COPC hierarchy, and flexible properties. Its reader must enter the same shared
`PointStream`, validation, filtering, LOD, tiling, and authoring path without
introducing LAS types into shared APIs.

`libs/usd-ply` already provides the tested PLY 1.0 header-inspection foundation
for ASCII, binary little-endian, and binary big-endian inputs. v0.4.0 completes
the point-focused reader and adds a thin `plugins/pointcloud-ply` adapter.

The shipped scope is scalar ASCII and binary vertex data, including binary
big-endian input; `x`, `y`, `z`, RGB, intensity, classification, and generic
scalar attributes; `PLYxxx` diagnostics; plugin discovery and stage-open smoke
tests; and shared PointStream and payload-backed tiled authoring. Metadata-only
inspection, faces, mesh authoring, PLY writing, and renderer-specific behavior
remain out of scope.

Faces, arbitrary non-vertex elements, mesh authoring, PLY writing, and
renderer-specific behavior are out of scope. PLY files without reliable
georeferencing must use explicit arguments or report its absence; they must
not invent a CRS.

### `v0.5.0` — remote COPC through OpenUSD asset resolution

`usdCopc` must consume a project-owned random-access byte source, not HTTP.
The OpenUSD-dependent plugin layer may adapt an `ArAsset` supplied by the
active `ArResolver`:

```text
Local file -> LocalRandomAccessSource -> usdCopc
ArResolver -> ArAsset adapter --------> usdCopc
```

The contract provides deterministic offset reads, source-size discovery,
explicit short-read and EOF behavior, and typed diagnostics. It contains no
OpenUSD types, implicit retries, or transport policy. Local COPC first moves
onto this same interface so remote reads are not a parallel path.

HTTP is resolver capability, not a bundled COPC feature. Remote COPC is
supported only when the active resolver can resolve the asset and provide
efficient random-access `ArAsset` reads, such as an HTTP resolver that uses
byte ranges. No HTTP client, cloud SDK, authentication flow, retry policy,
generic network cache, source upload, or COPC writing belongs in this release.

Tests use an instrumented project-owned source to prove selective header,
hierarchy-page, and selected-point range reads, then verify equivalent local
and resolver-backed point streams and authored USD. Resolver-backed failures
map to project-owned diagnostics.

Generated-USDC caching remains separate from source byte-range caching. A
resolver or transport may cache bytes; `usdGeoCache` caches generated USD.
Resolver-backed cache identity is conservative: it requires stable
resolver-provided identity inputs such as a resolved identifier, size,
validation token, or digest. Cache reuse is disabled when that identity is not
sufficiently stable rather than risking stale generated output. v0.6.0 adds
explicit lookup states, statistics, diagnostics, and recovery for corrupt or
interrupted generated-cache entries.

## Phases

| Phase | Scope | Status | Notes |
| --- | --- | --- | --- |
| 0 | Technical validation and plugin dependencies | Complete | Reader/plugin split, precision path, fixtures, and CI are shipped |
| 1 | Shared geospatial, cache, point-cloud, and authoring contracts | Complete | `usdGeoCore`, `usdGeoCache`, `usdPointCloudCore`, and authoring contracts are shipped |
| 2 | Direct LAS loading and `UsdGeomPoints` | Complete | LAS 1.2-1.4, formats 0-10, CRS, waveform metadata, and Extra Bytes are shipped |
| 3 | LAZ, arguments, streaming, and derived-USDC cache | Complete | Chunk decoding, normalized arguments, cache lookup, and conversion integration are shipped |
| 4 | Tiling, LOD, and local COPC | Complete in `v0.3.0` | Spill-backed tiled authoring and local COPC hierarchy reads share the common contracts |
| 5 | PLY point-cloud read support | Complete in `v0.4.0` | Bounded scalar decoding and payload-backed tiled plugin reads share the common contracts |
| 6 | Resolver-backed COPC random access | Released in `v0.5.0` | The plugin adapts resolver-opened `ArAsset` values to the project-owned source interface; remote cache reuse is conservative |
| 7 | Source identity and cache hardening | Released in `v0.6.0` | Keep generated-output cache identity format- and transport-independent |
| 8 | Point-budget-aware adaptive tiling | Planned for `v0.7.0` | Preserve fixed-grid behavior while adding deterministic adaptive planning |
| 9 | E57 and other point-cloud formats | Deferred | Reuse `PointStream`, processing, authoring, and cache contracts |

## Workstreams

The policy orders work by capability rather than by format. Each workstream
maps onto the phases above.

| Workstream | Scope | Phases | Status |
| --- | --- | --- | --- |
| W1 | Public specification alignment, typed diagnostics, endian-safe decoding | 0, 1 | Complete |
| W2 | LAS, LAZ, CRS, Extra Bytes, filters, streaming, and cache | 2, 3 | Complete |
| W3 | Shared tile/LOD contracts, spill-backed tiling, and conversion | 4 | Complete |
| W4 | Local COPC hierarchy, partial reads, and FileFormat integration | 4 | Complete in `v0.3.0` |
| W5 | Documentation consolidation | `v0.3.x` | Complete |
| W6 | PLY decoding and plugin integration | 5 | Complete in `v0.4.0` |
| W7 | Random-access source and resolver-backed COPC | 6 | Released in `v0.5.0` |
| W8 | Source identity and generated-cache hardening | 7 | Released in `v0.6.0` |
| W9 | Point-budget-aware adaptive tiling | 8 | Planned for `v0.7.0` |

The completed workstreams established the shared point schema, streaming reader
API, tile/LOD representation, and local COPC path. PLY must consume those
contracts rather than create a second point authoring route. Remote COPC then
extends only the source boundary; it must not make transport part of COPC
parsing.

## Documents

- [Infrastructure maturity](infrastructure-maturity.md)
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

## Future Repository Candidates

- `usd-terrain-plugins`: GeoTIFF elevation, DEM, COG, heightmaps, and terrain meshes
- `usd-vector-plugins`: GeoJSON, FlatGeobuf, GeoPackage, and Shapefile
