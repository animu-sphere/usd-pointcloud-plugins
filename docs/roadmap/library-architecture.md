# Library Architecture

## Goals

- Keep format-independent geospatial contracts reusable across all plugins.
- Keep OpenUSD types out of decoders and low-level data models.
- Build each format as an independent OpenStrata bundle.
- Allow direct file access and offline cache generation to share the same readers.
- Prevent large optional dependencies from becoming mandatory for unrelated formats.

## Planned Libraries

| Library | Responsibility | Required dependencies | Must not contain |
| --- | --- | --- | --- |
| `usdGeoCore` | CRS metadata, units, local origins, spatial bounds, tile IDs, cache keys, diagnostics | C++ standard library | OpenUSD, LAS, GDAL, or renderer-specific types |
| `usdGeoUsd` | OpenUSD metadata, API schemas, common prim authoring, tile and LOD stage representation | `usdGeoCore`, OpenUSD | Format parsing or compression codecs |
| `usdPointCloudCore` | Format-independent point attributes, chunk contracts, sampling inputs, point-cloud validation | `usdGeoCore` | LAS, E57, or OpenUSD types |
| `usdLas` | LAS headers, VLR/EVLR records, point records, Scale/Offset decoding, CRS extraction | `usdGeoCore`, `usdPointCloudCore` | OpenUSD stage authoring |
| `usdLaz` | LAZ decompression behind the same logical reader contract as LAS | `usdLas`, selected LAZ codec | OpenUSD stage authoring |
| `usdCopc` | COPC hierarchy, octree keys, byte ranges, and resolution mapped onto shared tile contracts | `usdLaz`, `usdGeoCore` | OpenUSD stage authoring or LAZ codec internals |
| `usdPly` | PLY element and property headers, binary and ASCII vertex payloads, generic property mapping | `usdGeoCore`, `usdPointCloudCore` | OpenUSD stage authoring or assumed CRS defaults |
| `usdAsciiPoints` | Delimited text point parsing, column mapping, header detection, unit and CRS inputs | `usdGeoCore`, `usdPointCloudCore` | OpenUSD stage authoring or format guessing without explicit arguments |
| `usdE57` | E57 scan model, per-scan pose, cartesian and spherical coordinates, per-scan attribute availability | `usdGeoCore`, `usdPointCloudCore`, selected E57 backend | OpenUSD stage authoring or LAS-specific record assumptions |
| `usdTerrainCore` | Raster windows, NoData, grid geometry, overview and terrain-tile contracts | `usdGeoCore` | GDAL or OpenUSD types in public APIs |
| `usdVectorCore` | Geometry and feature-property contracts, layer metadata, spatial-query inputs | `usdGeoCore` | OGR or OpenUSD types in public APIs |
| `usdGeoCache` | Stable cache keys, USDC tile layout, cache lookup and invalidation | `usdGeoCore`, `usdGeoUsd`, OpenUSD | Format-specific decoding |

Library names are logical targets. They may initially live in the monorepo under `libs/`, but each must remain independently buildable and packageable where practical.

`usdCopc` is a candidate, not a commitment. The alternative is an isolated COPC
module inside `usdLaz`. Either way, COPC-specific structures never leak into the
LAZ chunk reader contract.

Inside `usdLas`, a single translation unit does not stay responsible for the
whole format. As coverage grows, it splits into header, metadata, point decode,
CRS, Extra Bytes, and waveform modules with the same public API.

## Planned Plugin Bundles

| Bundle | File extensions | Internal libraries |
| --- | --- | --- |
| `plugins/geo-las` | `.las` | `usdLas`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-laz` | `.laz` | `usdLaz`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-ply` | `.ply` | `usdPly`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-points-text` | `.xyz`, `.pts`, `.csv` when explicitly selected | `usdAsciiPoints`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-e57` | `.e57` | `usdE57`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-geotiff` | `.tif`, `.tiff` | `usdTerrainCore`, `usdGeoUsd` |
| `plugins/geo-geojson` | `.geojson`, `.json` when explicitly selected | `usdVectorCore`, `usdGeoUsd` |

`.csv` and `.json` are generic extensions. Those bundles never claim the
extension by default; a host opts in through an explicit file-format selection,
and the readers require the arguments they need instead of inferring a layout.

Each FileFormat Plugin remains a thin adapter. It normalizes Sdf file-format arguments, invokes a reader, and authors the resulting layer through `usdGeoUsd`.

## Dependency Direction

```text
usdGeoCore
  |-- usdGeoUsd ------------------------ usdGeoCache
  |-- usdPointCloudCore -- usdLas -- usdLaz -- usdCopc
  |                     |-- usdPly
  |                     |-- usdAsciiPoints
  |                     `-- usdE57
  |-- usdTerrainCore
  `-- usdVectorCore

readers + usdGeoUsd -> independent FileFormat Plugin bundles
readers + usdGeoCache -> usd-geo-tools
```

Dependencies only flow from specialized modules toward shared contracts. `usdGeoCore` never depends on a format reader, OpenUSD, PROJ, GDAL, or a compression codec. Optional integrations use private implementation types or adapter libraries.

## Repository Shape

```text
libs/
  usd-geo-core/
  usd-geo-usd/
  usd-pointcloud-core/
  usd-las/
  usd-laz/
  usd-copc/
  usd-ply/
  usd-ascii-points/
  usd-e57/
  usd-terrain-core/
  usd-vector-core/
  usd-geo-cache/
plugins/
  geo-las/
  geo-laz/
  geo-ply/
  geo-points-text/
  geo-e57/
  geo-geotiff/
  geo-geojson/
tools/
  usdgeo/
tests/
  integration/
  data/
```

Only create a directory when its first tested capability is implemented. The shape describes ownership boundaries, not a requirement to scaffold empty modules.

## API Rules

1. Public APIs use project-owned value types and standard-library types.
2. Source coordinates and stage-local coordinates use distinct names and explicit transforms.
3. Readers return deterministic, validated intermediate data and do not author USD directly.
4. File-format arguments are normalized before reader or cache lookup.
5. Unknown source metadata is preserved or reported; it is never silently discarded.
6. Tile, LOD, and cache contracts are shared, while format-specific spatial indexes remain private.
7. Write support is deferred until read behavior and preservation rules are stable.

## Build and Packaging

- Use OpenStrata manifests for libraries and plugin bundles.
- Keep the root CMake build working without `ost` for local development.
- Keep large dependencies optional and scoped to the owning target.
- Test pure libraries without requiring an OpenUSD runtime where possible.
- Validate plugin bundles with the pinned OpenStrata `cy2026` / `usd` runtime.
