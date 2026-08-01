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
- [ ] OpenUSD FileFormat Plugin PoC
- [ ] LAS reader
- [ ] LAZ reader
- [ ] Tile / LOD
- [ ] USDC cache

## Next Implementation Sequence

1. Complete finite-value validation and explicit local-coordinate transforms in `usdGeoCore`.
2. Add deterministic tile IDs and normalized cache-key inputs to `usdGeoCore`.
3. Create `usdPointCloudCore` with point-attribute, chunk, and validation contracts.
4. Create `usdGeoUsd` with a minimal `UsdGeomPoints` layer writer and metadata authoring.
5. Implement LAS metadata inspection and uncompressed point-record decoding in `usdLas`.
6. Compose `usdLas` and `usdGeoUsd` in the `geo-las` FileFormat Plugin PoC.

Do not begin LAZ integration until the LAS conformance fixtures pass through both the reader-only tests and the FileFormat Plugin integration test.
