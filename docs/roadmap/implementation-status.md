# Implementation Status

This is the checklist view of what `main` implements. The source-level support
matrix is in [capability matrix](../reference/CAPABILITY_MATRIX.md); the
ordered plan is in [README.md](README.md).

## Shipped in v0.1.0 (2026-08-01)

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
- [x] Create the `UsdGeomPoints` authoring and metadata round-trip library
- [x] Add `usdLas` LAS 1.2-1.4 header inspection and uncompressed point decoding
- [x] OpenUSD FileFormat Plugin PoC (LAS discovery and UsdGeomPoints authoring path)
- [x] LAS reader integration and deterministic conformance fixture
- [x] LAS VLR/EVLR inspection and WKT CRS extraction
- [x] Select laz-perf as the initial LAZ codec and define the chunk reader contract
- [x] Add the laz-perf adapter behind the `usdLaz` chunk reader contract
- [x] Add the LAZ reader conformance fixture and FileFormat Plugin integration
- [x] Record the standing design policy
- [x] Document the exact point format, attribute, VLR, and CRS support matrix
- [x] Document known limitations and plugin usage in the README
- [x] Document the typed diagnostics contract and its migration
- [x] Document LGPL-compliant binary distribution and OpenUSD compatibility

## Unreleased on `main`

### Readers and decoding

- [x] Endian-safe binary decoding
- [x] Add typed diagnostics to `usdGeoCore` and LAS / LAZ reader APIs
- [x] Move the LAS / LAZ FileFormat Plugins onto the typed diagnostics path
- [x] GeoTIFF CRS VLR parsing (structured VLR metadata)
- [x] Extra Bytes descriptors and scalar generic point attributes
- [x] Remaining LAS 1.4 point attributes, including NIR
- [x] Waveform contract and LAS point formats 4, 5, 9, and 10
- [x] Chunked and range-based reader API
- [x] Metadata-only LAS and LAZ reads

### Plugin adapters

- [x] Record the plugin adapter and file-format argument contracts
- [x] Move `geospatial-las` onto `usdlas::LasReader`
- [x] Add the preliminary `usdgeo::AuthorPointCloudAsset` authoring API
- [x] Move the shared authoring tail into `usdgeo::AuthorPointCloudAsset`
- [x] Normalize file-format arguments and pass read options through the plugins

### Tiling and LOD

- [x] Record the OpenUSD 26.08 `usdLod` tile and LOD contract
- [x] Shared LOD contracts (`PointTileId`, `PointLodItem`, `PointLodHierarchy`)
- [x] LOD validation invariants and typed diagnostics
- [x] Deterministic, versioned fixed-stride point sampling and cache-key inputs
- [x] `usdLod` authoring in the authoring library (single non-tiled root)
- [x] LOD file-format arguments (compact profiles)
- [x] Spatial tiling and per-tile LOD roots (authoring API)
- [x] Payload packaging (authoring API)

### Structure and documentation

- [x] Rename `usd-geo-usd` to `usd-pointcloud-authoring`
      (`usdPointCloudAuthoring` to `usdPointCloudAuthoring`)
- [x] Rename `plugins/geospatial-las` and `plugins/geospatial-laz` to `plugins/geospatial-las`
      and `plugins/geospatial-laz`, including CMake targets, shared library
      names, `plugInfo.json` type names, and CI paths
- [x] Reorganize `docs/` by responsibility and add the documentation index
- [x] Promote the library architecture into a binding
      [workspace contract](../architecture/WORKSPACE.md)
- [x] Add a `README.md` for every module under `libs/` and `plugins/`
- [x] Record the rename in [migration](../compatibility/MIGRATION.md)

## Open

### Next: bounded-memory streaming and spatial tiling

The plan is [streaming and tiling](streaming-and-tiling.md).

- [x] `PointStream` pull interface in `usdPointCloudCore`
- [x] `usdPointCloudTiling`: fixed-grid tile keys, configuration, and source-coordinate tile router
- [x] Spool schema, thresholds, cleanup, and deterministic iteration order
- [ ] LAS `PointStream` connected to tiled payload authoring
- [ ] LAZ `PointStream` connected to tiled payload authoring
- [ ] Spatial `tile` file-format arguments enabled through the plugins
- [ ] Bounded-memory tests and generated large-corpus spill coverage
- [ ] Streaming benchmarks and their documented commands and datasets

### Other open work

- [ ] Payload working-set measurement across scene and render delegates
- [ ] Deterministic USDC cache generation and lookup
- [ ] Extra Bytes descriptor-name normalization contract
- [ ] Vector Extra Bytes types
- [ ] Bounds and classification filter arguments
- [ ] EPSG inference and conflicting-CRS detection
- [ ] Declare OST test fixtures in both bundle manifests so the L3 and L4
      checks stop skipping
- [ ] Stage licensing, notice, capability, compatibility, and installation
      documents into release assets
- [ ] Decide whether the plugins should also become dynamic file formats
      ([ADR 0003](../adr/0003-dynamic-file-format.md))

## Notes

The sampling contract uses a versioned fixed-stride selection that preserves
source order and applies the same indices to every populated point attribute.
Its algorithm, version, and target count are normalized cache-key inputs.

Argument normalization makes the streaming reader's chunk and point-range
controls reachable through the plugin layer, and attribute selection is
normalized before authoring; see the
[plugin adapter contract](../architecture/PLUGIN_ADAPTER.md) and the
[file-format argument contract](../architecture/FILE_FORMAT_ARGUMENTS.md).

Compact `lod` profiles author a single non-tiled `usdLod` root through the
shared authoring path for LAS and LAZ. The authoring library additionally
supports deterministic per-tile LOD roots and payload-backed LOD children, but
direct LAS and LAZ FileFormat reads do not yet stream decoded chunks into tile
payloads. Keeping that distinction explicit is a documentation requirement,
not a stylistic preference.

The LAS conformance fixture and FileFormat Plugin integration gate passed
before LAZ integration. The LAZ reader uses the same point-cloud authoring path
and validates chunked decoder output through its own FileFormat Plugin
integration fixture. Documented support is tracked in
[capability matrix](../reference/CAPABILITY_MATRIX.md), which is updated in the
same change as any decoder that widens it.
