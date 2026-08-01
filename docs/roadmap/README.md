# usd-geo-plugins Roadmap

This directory contains documents that break the implementation policy into actionable milestones.

## Principles

- Keep `usd-geo-core` limited to shared CRS, origin, unit, bounds, and tile primitives.
- Keep OpenUSD authoring in `usdGeoUsd`, separate from pure data models and readers.
- Put format-specific readers in independent modules under `plugins`.
- Complete a read-first LAS / LAZ vertical slice first.
- Keep source coordinates and USD stage-local coordinates explicitly separate.
- Produce small, testable deliverables at every stage.

## Phases

| Phase | Scope | Status |
| --- | --- | --- |
| 0 | Technical validation of the FileFormat Plugin and dependencies | In progress |
| 1 | `usdGeoCore`, `usdGeoUsd`, and point-cloud contracts | In progress |
| 2 | Direct LAS loading and `UsdGeomPoints` | Not started |
| 3 | LAZ, attribute selection, and USDC caching | Not started |
| 4 | Spatial tiles, LOD, and COPC | Not started |
| 5 | GeoTIFF and COG terrain | Not started |
| 6 | GeoJSON and FlatGeobuf vector data | Not started |
| 7 | E57 and multi-scan point clouds | Not started |
| 8 | GeoPackage, Shapefile, and additional interoperability formats | Deferred |

## Documents

- [Phase 0](phase-0-technical-validation.md)
- [Phase 1](phase-1-geo-core.md)
- [Library architecture](library-architecture.md)
- [File format support order](format-support-order.md)
- [Coordinate model decision](adr-0001-coordinate-model.md)
- [LAZ codec decision](adr-0002-laz-codec.md)
- [Implementation status](implementation-status.md)
