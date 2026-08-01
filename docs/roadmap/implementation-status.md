# Implementation Status

## 2026-08-01

- [x] Add root CMake to the empty repository
- [x] Add the minimal `usd-geo-core` library structure
- [x] Add `SpatialBounds` validation, expansion, center, and size operations
- [x] Add the `GeoReference` contract for CRS, units, up axis, and origin
- [x] Add core unit tests
- [x] Add OpenStrata workspace and `usdGeoCore` library manifests
- [x] Generate and validate the OpenStrata runtime lockfile
- [x] Build and test the core through `ost`
- [x] Define library ownership and dependency direction
- [x] Define the file-format support order and entry gates
- [x] Add finite-value validation and explicit local-coordinate transforms
- [x] Add deterministic tile IDs and normalized cache-key inputs
- [x] Create `usdPointCloudCore` point-attribute and chunk contracts
- [ ] OpenUSD FileFormat Plugin PoC
- [ ] LAS reader
- [ ] LAZ reader
- [ ] Tile / LOD
- [ ] USDC cache

## Next Implementation Sequence

1. Create `usdGeoUsd` with a minimal `UsdGeomPoints` layer writer and metadata authoring.
2. Implement LAS metadata inspection and uncompressed point-record decoding in `usdLas`.
3. Compose `usdLas` and `usdGeoUsd` in the `geo-las` FileFormat Plugin PoC.

Do not begin LAZ integration until the LAS conformance fixtures pass through both the reader-only tests and the FileFormat Plugin integration test.
