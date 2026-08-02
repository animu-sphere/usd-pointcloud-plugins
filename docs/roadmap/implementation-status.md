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
- [x] Create `usdGeoUsd` `UsdGeomPoints` authoring and metadata round-trip
- [x] Add `usdLas` LAS 1.2-1.4 header inspection and uncompressed point decoding
- [x] OpenUSD FileFormat Plugin PoC (LAS discovery and UsdGeomPoints authoring path)
- [x] LAS reader integration and deterministic conformance fixture
- [x] LAS VLR/EVLR inspection and WKT CRS extraction
- [x] Select laz-perf as the initial LAZ codec and define the chunk reader contract
- [x] Add the laz-perf adapter behind the `usdLaz` chunk reader contract
- [x] Add the LAZ reader conformance fixture and FileFormat Plugin integration
- [x] Record the standing development policy
- [x] Document the exact point format, attribute, VLR, and CRS support matrix
- [x] Document known limitations and plugin usage in the README
- [x] Document the typed diagnostics contract and its migration
- [x] Document LGPL-compliant binary distribution and OpenUSD compatibility
- [x] Endian-safe binary decoding
- [x] Add typed diagnostics to `usdGeoCore` and LAS / LAZ reader APIs
- [x] Move the LAS / LAZ FileFormat Plugins onto the typed diagnostics path
- [x] GeoTIFF CRS VLR parsing (structured VLR metadata)
- [x] Extra Bytes descriptors and scalar generic point attributes
- [x] Remaining LAS 1.4 point attributes, including NIR
- [x] Waveform contract and LAS point formats 4, 5, 9, and 10
- [x] Chunked and range-based reader API
- [x] Record the OpenUSD 26.08 `usdLod` tile and LOD contract
- [x] Record the plugin adapter and file-format argument contracts
- [x] Move `geo-las` onto `usdlas::LasReader`
- [x] Add the preliminary `usdgeo::AuthorPointCloudAsset` authoring API
- [x] Move the shared authoring tail into `usdgeo::AuthorPointCloudAsset`
- [x] Normalize file-format arguments and pass read options through the plugins
- [x] Shared LOD contracts (`PointTileId`, `PointLodItem`, `PointLodHierarchy`)
- [x] LOD validation invariants and typed diagnostics
- [x] Deterministic, versioned fixed-stride point sampling and cache-key inputs
- [x] `usdLod` authoring in `usdGeoUsd` (single non-tiled root)
- [x] LOD file-format arguments (compact profiles)
- [x] Spatial tiling and per-tile LOD roots
- [x] Payload packaging
- [ ] Payload working-set measurement
- [x] Metadata-only reads
- [ ] USDC cache

## Next Implementation Sequence

1. Measure stage population and payload working sets across the supported
	OpenUSD scene and render delegates.
2. Add deterministic USDC cache generation and lookup after the measured
	payload behavior is documented.

The initial sampling contract uses a versioned fixed-stride selection that
preserves source order and applies the same indices to every populated point
attribute. Its algorithm, version, and target count are normalized cache-key
inputs.

Argument normalization now makes the streaming reader's chunk and point-range
controls reachable through the plugin layer. Attribute selection is also
normalized before authoring; see the
[plugin adapter contract](../architecture/plugin-adapter.md) and the
[file-format argument contract](../architecture/file-format-arguments.md).

The shared reader API stabilizes the point schema and streaming path before
tile / LOD work begins. Its current contract is documented in
[point reader architecture](../architecture/point-reader.md), and the LOD
target is fixed in the [tile and LOD contract](../architecture/lod.md).

Compact `lod` profiles now author a single non-tiled `usdLod` root through the
shared authoring path for LAS and LAZ. The USD authoring bridge also supports
deterministic per-tile LOD roots and payload-backed LOD children. Payload
working-set behavior remains unmeasured, and metadata-only reads and USDC
cache generation remain open. Whether the plugins should also become dynamic
file formats is open; see
[ADR-0003](adr-0003-dynamic-file-format.md).

The LAS conformance fixture and FileFormat Plugin integration gate passed
before LAZ integration. The LAZ reader now uses the same point-cloud authoring
path and validates chunked decoder output through a FileFormat Plugin
integration fixture. Documented support is tracked in
[supported formats](../supported-formats.md), which is updated in the same
change as any decoder that widens it.
