# usd-geo-plugins Roadmap

This directory contains documents that break the
[development policy](../development-policy.md) into actionable milestones.
The policy states the standing direction; this directory states the order of
work.

## Principles

- Keep `usd-geo-core` limited to shared CRS, origin, unit, bounds, tile, and
  diagnostic primitives.
- Keep OpenUSD authoring in `usdGeoUsd`, separate from pure data models and readers.
- Put format-specific readers in independent modules under `plugins`.
- Complete a read-first LAS / LAZ vertical slice first.
- Keep source coordinates and USD stage-local coordinates explicitly separate.
- Produce small, testable deliverables at every stage.

## Phases

| Phase | Scope | Status | Notes |
| --- | --- | --- | --- |
| 0 | Technical validation of the FileFormat Plugin and dependencies | In progress | PoC, adapter split, and precision path are done; large-data timing and memory measurements are outstanding |
| 1 | `usdGeoCore`, `usdGeoUsd`, and point-cloud contracts | In progress | Shipped in v0.1.0 except `usdGeoCache` |
| 2 | Direct LAS loading and `UsdGeomPoints` | In progress | Point formats 0-3 and 6-8 land; formats 4, 5, 9, 10 and the remaining attributes are open |
| 3 | LAZ, attribute selection, and USDC caching | In progress | LAZ chunk decoding shipped; attribute selection and the cache are not started |
| 4 | Spatial tiles, LOD, and COPC | Not started | Blocked on the shared tile contract |
| 5 | PLY and delimited text point clouds (XYZ, PTS, CSV) | Not started | Needs the generic attribute model and file-format arguments |
| 6 | E57 and multi-scan point clouds | Not started | Extends the point-cloud contracts to several scans per file |
| 7 | GeoTIFF, DEM, and COG terrain | Not started | First non-point-cloud domain |
| 8 | GeoJSON and FlatGeobuf vector data | Not started | |
| 9 | GeoPackage, Shapefile, and additional interoperability formats | Deferred | |

## Workstreams

The policy orders work by capability rather than by format. Each workstream
maps onto the phases above.

| Workstream | Scope | Phases |
| --- | --- | --- |
| W1 | Public specification alignment, typed diagnostics, endian-safe decoding | 0, 1 |
| W2 | LAS attribute coverage, GeoTIFF CRS, Extra Bytes | 2 |
| W3 | Point formats 4, 5, 9, 10 and the waveform contract | 2 |
| W4 | Chunked and range-based reader API, memory budget, filtering | 3 |
| W5 | Shared tile and LOD contracts, OpenUSD LOD mapping | 4 |
| W6 | USDC cache, COPC, PLY, delimited text, E57, terrain rasters, remote byte-range sources | 3-7 |

W1 through W4 stabilize the shared point schema and the streaming reader API.
Both W3 and W5 depend on them, so they are not started before those contracts
settle.

## Documents

- [Phase 0](phase-0-technical-validation.md)
- [Phase 1](phase-1-geo-core.md)
- [Library architecture](library-architecture.md)
- [File format support order](format-support-order.md)
- [Coordinate model decision](adr-0001-coordinate-model.md)
- [LAZ codec decision](adr-0002-laz-codec.md)
- [Implementation status](implementation-status.md)

Related documents outside this directory:

- [Development policy](../development-policy.md)
- [Supported formats](../supported-formats.md)
- [Diagnostics contract](../architecture/diagnostics.md)
- [OpenUSD compatibility](../compatibility/openusd.md)
- [Binary distribution and licensing](../distribution.md)
