# Phase 1: Shared Foundations

## Objective

Provide the minimum geospatial model shared by point clouds, terrain, and vector data, plus a separate OpenUSD authoring bridge. Do not place format-specific types or readers in `usdGeoCore`, and do not place OpenUSD types in reader APIs.

## Implementation Order

1. `Vec3d` and `SpatialBounds`
2. Preserve CRS, units, up axis, and local origin through `GeoReference`
3. Define the coordinate transform input and output contract
4. Add tile IDs and spatial hierarchy
5. Add normalized cache keys
6. Add format-independent point-cloud attribute and chunk contracts in `usdPointCloudCore`
7. Add OpenUSD metadata, API Schema, and common authoring in `usdGeoUsd`
8. Add shared typed diagnostic value types in `usdGeoCore`
9. Add cache lookup and USDC tile-layout contracts in `usdGeoCache`

Steps 1 through 7 shipped in v0.1.0, except the API Schema, which stays
deferred until the tile and LOD representation is settled. Steps 8 and 9 are
open; see the [diagnostics contract](../architecture/diagnostics.md).

## Contracts

- Preserve input coordinates as `double`.
- Treat local-origin selection as explicit configuration; never change it implicitly.
- Treat EPSG as an identifier and WKT or PROJJSON as the authoritative representation.
- Distinguish an invalid `SpatialBounds` from a finite, valid bounds.
- Do not expose external-library types in the public API.
- Keep `usdGeoCore` independent of OpenUSD and format libraries.
- Keep readers independent of `usdGeoUsd`; plugin adapters compose both sides.
- Report failures as typed diagnostics with stable codes; do not throw across a
  reader API boundary.

## Tests

- Bounds calculations with large absolute coordinates
- Empty bounds and single-point bounds
- Invalid up axis and missing CRS
- Reconstruction error from a local origin
- Tile ID normalization and reproducibility
- Point-attribute validation without an OpenUSD runtime
- Equivalent metadata authored through `usdGeoUsd` and reconstructed from a stage
- Stable cache keys for equivalent normalized file-format arguments
- Typed diagnostics carrying stable codes, severity, and byte or point anchors

## Exit Criteria

- `usdGeoCore` and `usdPointCloudCore` build and test without OpenUSD.
- `usdGeoUsd` authors CRS, local-origin, bounds, and point attributes into a test layer.
- Public headers do not expose PROJ, GDAL, LAS codec, or OpenUSD types outside their owning libraries.
- The LAS plugin can depend on reader and authoring libraries without either depending on the plugin.
