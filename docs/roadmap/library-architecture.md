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
| `usdTerrainCore` | Raster windows, NoData, grid geometry, overview and terrain-tile contracts | `usdGeoCore` | GDAL or OpenUSD types in public APIs |
| `usdVectorCore` | Geometry and feature-property contracts, layer metadata, spatial-query inputs | `usdGeoCore` | OGR or OpenUSD types in public APIs |
| `usdGeoCache` | Stable cache keys, USDC tile layout, cache lookup and invalidation | `usdGeoCore`, `usdGeoUsd`, OpenUSD | Format-specific decoding |

Library names are logical targets. They may initially live in the monorepo under `libs/`, but each must remain independently buildable and packageable where practical.

## Planned Plugin Bundles

| Bundle | File extensions | Internal libraries |
| --- | --- | --- |
| `plugins/geo-las` | `.las` | `usdLas`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-laz` | `.laz` | `usdLaz`, `usdPointCloudCore`, `usdGeoUsd` |
| `plugins/geo-geotiff` | `.tif`, `.tiff` | `usdTerrainCore`, `usdGeoUsd` |
| `plugins/geo-geojson` | `.geojson`, `.json` when explicitly selected | `usdVectorCore`, `usdGeoUsd` |
| `plugins/geo-e57` | `.e57` | a dedicated E57 reader, `usdPointCloudCore`, `usdGeoUsd` |

Each FileFormat Plugin remains a thin adapter. It normalizes Sdf file-format arguments, invokes a reader, and authors the resulting layer through `usdGeoUsd`.

## Dependency Direction

```text
usdGeoCore
  |-- usdGeoUsd ------------------------ usdGeoCache
  |-- usdPointCloudCore -- usdLas -- usdLaz
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
  usd-terrain-core/
  usd-vector-core/
  usd-geo-cache/
plugins/
  geo-las/
  geo-laz/
  geo-geotiff/
  geo-geojson/
  geo-e57/
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
