# Changelog

All notable changes to this project are documented here.

## [0.3.0] - 2026-08-11

### Added

- Local, read-only COPC metadata and hierarchy validation, selective point-data
  range reads, and native hierarchy point streaming through the shared LAZ
  decoder.
- The `pointcloud-copc` OpenUSD FileFormat Plugin for `.copc` inputs, including
  metadata-only, non-tiled, and hierarchy-backed tiled reads.
- Equivalence coverage across LAS, LAZ, and COPC point streams and authored LOD
  representations.
- Deterministic USDC cache generation for the conversion tool and cache lookup
  for direct LAS, LAZ, and COPC FileFormat reads through `USDGEO_CACHE_ROOT`.

### Changed

- Release products now include the LAS, LAZ, and COPC plugin bundles.
- COPC native hierarchy metadata is mapped to the shared tile and `usdLod`
  authoring contracts.

### Compatibility

- Existing LAS and LAZ format ids, file-format arguments, and authored stage
  shape remain compatible with v0.2.2.
- COPC support is local-only and requires OpenUSD 26.08 through the existing
  `cy2026` / `usd` runtime contract.

### Known limitations

- COPC writing, HTTP range sources, network caching, and new public USD schemas
  remain deferred.
- Writing LAS, LAZ, or COPC is out of scope; all three plugins export as
  `usda`.

## [0.2.2] - 2026-08-06

### Fixed

- Corrected the OpenStrata workspace, plugin manifests, and plugin CMake
  project versions so platform packages and product archives are named
  `0.2.2`.
- Added release metadata validation to the release workflow and documented the
  version update and tag order.

### Compatibility

- No runtime, file-format, or public API behavior changed from v0.2.1.

## [0.2.1] - 2026-08-06

### Added

- Bounds and classification filters for LAS and LAZ reads, normalized into
  canonical file-format arguments and applied in source coordinates.
- EPSG inference from explicit WKT and GeoTIFF horizontal CRS keys, with typed
  conflict diagnostics when CRS definitions disagree.
- An explicit LAS/LAZ conversion tool for tiled, payload-backed generation with
  deterministic manifest output.
- Real-dataset processing, RSS, spool, payload, and payload working-set
  measurements for the documented LAS and LAZ paths.

### Changed

- The explicit conversion tool is now the production entry point for
  long-running tiled generation; static FileFormat tiling remains available for
  preview, compatibility, and small inputs.
- Release and capability documentation now distinguishes implemented paths,
  measured paths, and remaining limitations.

### Fixed

- Cleanup of tiled authoring after cancellation and failure, including spool
  reader closure and payload-directory restrictions.
- Recovery of interrupted conversion transactions, making tiled conversion
  interruption-safe, and deterministic root and manifest publication.

### Known limitations

- Rendering is provided by the consuming OpenUSD application.
- Tiling uses a fixed-grid `tileSize`; adaptive depth and point-budget tile
  planning are not exposed through LAS/LAZ file-format arguments.
- A deterministic USDC cache is not implemented, and broader real-world
  dataset coverage remains open.
- Waveform sample data is retained as packet metadata and external `.wdp`
  references but is not fetched or interpreted.
- Writing LAS or LAZ is out of scope; the plugins export `usda`.

## [0.2.0] - 2026-08-05

### Changed — breaking

- Renamed `libs/usd-geo-usd` to `libs/usd-pointcloud-authoring`. The library id
  and CMake package become `usdPointCloudAuthoring` and the CMake target becomes
  `usdpointcloud::authoring`. `usdgeo::usd` remains as a deprecated alias and is
  removed in v0.3.0.
- Renamed `plugins/geo-las` to `plugins/geospatial-las` and `plugins/geo-laz` to
  `plugins/geospatial-laz`. The OpenStrata bundle names, `plugin/resources/`
  directories, CMake targets, installed shared libraries, and `plugInfo.json`
  type names all move to `geospatial-*` / `UsdGeoLasFileFormat` /
  `UsdGeoLazFileFormat`.
- Renamed the per-bundle header directories and diagnostic namespaces from
  `geolas` / `geolaz` to `usdgeolas` / `usdgeolaz`.

No behavior changed with the rename: the `las` and `laz` format ids, the
`.las` / `.laz` extensions, every `LASxxx` / `LAZxxx` diagnostic code, every
file-format argument, and the authored stage are all unchanged. Consumers must
update `PXR_PLUGINPATH_NAME`, bundle paths, and any script naming the shared
libraries directly. The complete before/after table and checklist are in
[docs/compatibility/MIGRATION.md](docs/compatibility/MIGRATION.md).

### Added

- Scalar LAS Extra Bytes point attributes, authored as `geo:<name>` (`double[]`)
  with descriptor scale and offset applied. Types 1-10 are supported; vector
  types, non-finite values, and integers not exactly representable as `double`
  are rejected.
- GeoTIFF CRS VLR parsing: key directory, double parameters, and ASCII
  parameters are parsed and retained on the LAS header.
- The remaining LAS 1.4 point attributes, including NIR, classification flags,
  scanner channel, scan angle, user data, and point source ID.
- LAS point formats 4, 5, 9, and 10 with the waveform packet metadata contract
  and the external `.wdp` reference.
- Metadata-only LAS and LAZ reads: `Read(metadataOnly=true)` authors the
  `/PointCloud` metadata namespace — source count, bounds, CRS, and
  available-attribute metadata — without decoding point records.
- A chunked and range-based reader API behind the shared `PointReadOptions`
  contract, with a memory budget and a cancellation callback.
- Normalized file-format arguments, with a canonical argument map that
  participates in layer identity. `attributes`, `chunkPointLimit`,
  `memoryBudgetBytes`, `rangeFirstPoint`, `rangePointCount`, and `lod` are
  reachable; unknown, out-of-range, and not-yet-implemented arguments are
  rejected with typed diagnostics.
- Shared LOD contracts in `usdPointCloudCore`: `PointTileId`, `PointLodItem`,
  `PointLodHierarchy`, `PointTile`, their validation invariants, and typed
  diagnostics.
- Deterministic, versioned fixed-stride point sampling. The algorithm, version,
  and target count are normalized cache-key inputs.
- OpenUSD 26.08 `usdLod` authoring: `UsdLodRootAPI` and
  `UsdLodScreenSizeHeuristic` on a single non-tiled root.
- Compact `lod` file-format profiles (`off`, `preview`, `balanced`, `quality`)
  reachable from both LAS and LAZ reads.
- Spatial tiled authoring with deterministic per-tile `usdLod` roots, through
  the authoring API.
- Payload-backed tiled authoring: one USDC payload per tile and LOD level, with
  portable relative asset paths, through the authoring API.
- Bounded-memory LAS and LAZ pull-stream integration for spill-backed,
  payload-backed tiled authoring, including spool cleanup and rollback on
  failure.
- Typed diagnostics (`usdgeo::Diagnostic`, `Severity`, `DiagnosticCode`) in
  `usdGeoCore`, emitted by both readers and projected onto the stable plugin
  code prefixes.
- Endian-safe LAS point decoding.
- A `README.md` for every module under `libs/` and `plugins/`, and the
  [module README contract](docs/contributing/MODULE_README_CONTRACT.md) that
  makes it part of the module contract.
- [docs/architecture/WORKSPACE.md](docs/architecture/WORKSPACE.md), the binding
  structural contract for module identities, dependency directions, artifact
  naming, and change invariants.
- [docs/roadmap/streaming-and-tiling.md](docs/roadmap/streaming-and-tiling.md),
  the plan for bounded-memory streaming: the `PointStream` interface,
  spill-backed fixed-grid tiling, spool and payload contracts, the spatial
  file-format argument surface, the pull-request sequence, testing
  requirements, and the definition of done.
- [docs/guides/BUILDING.md](docs/guides/BUILDING.md) and
  [docs/guides/INSTALL.md](docs/guides/INSTALL.md).
- [docs/compatibility/MIGRATION.md](docs/compatibility/MIGRATION.md).
- Tile and LOD contract fixing OpenUSD 26.08 `usdLod` as the only public LOD
  representation, with the target namespace, validation invariants, sampling
  and cache-key rules, payload policy, and the test matrix.
- Plugin adapter contract recording the thin-adapter rule and its migration.
- File-format argument contract covering syntax, candidate arguments,
  validation and normalization rules, layer identity, and cache-key
  participation.
- ADR-0003 proposing a sequence for dynamic file format support, with the open
  questions that must be answered before it is accepted or rejected.
- Standing design policy covering contracts, LAS/LAZ scope, tiling, streaming,
  caching, diagnostics, binary safety, testing, and licensing.
- Capability matrix with the exact point format, attribute, VLR, CRS, and
  authored USD matrices.
- Diagnostics contract describing the migration from string errors to typed
  diagnostics.
- Binary distribution document covering the LGPL-2.1 obligations for
  `geospatial-laz`.
- OpenUSD compatibility statement for the pinned runtime and tested platforms.
- Roadmap entries for COPC, PLY, delimited text point formats, E57, GeoTIFF and
  DEM elevation, and COG.

### Changed

- Both FileFormat Plugins now meet the thin-adapter rule. `geospatial-las` runs
  on `usdlas::LasReader`, both plugins normalize arguments before reading, and
  both author through the shared `usdgeo::AuthorPointCloudAsset` entry point.
  Point fan-out, chunk-schema construction, CRS and bounds conversion, stage
  metrics, and layer transfer no longer live in a plugin.
- Reorganized `docs/` by responsibility — `architecture/`, `reference/`,
  `guides/`, `design/`, `adr/`, `compatibility/`, `contributing/`, `roadmap/`,
  `releases/` — and added [docs/README.md](docs/README.md) as the index.
  `development-policy.md`, `supported-formats.md`, `distribution.md`, and
  `roadmap/library-architecture.md` moved to `design/DESIGN_POLICY.md`,
  `reference/CAPABILITY_MATRIX.md`, `guides/DISTRIBUTION.md`, and
  `architecture/WORKSPACE.md`.
- Documentation now reports authoring-library capability and direct LAS/LAZ
  FileFormat read capability separately, because they differ: tiled and
  payload-backed authoring exist in the library, and no file-format argument
  reaches them yet.
- The README no longer claims that Extra Bytes point attributes are
  unimplemented, or that payload packaging is entirely unimplemented. Both
  claims were wrong.
- The README distinguishes the latest release from the current branch.
- Design policy section 5 replaces the open tile/LOD plan with the `usdLod`
  standing policy: no repository-specific LOD schema, tiling separate from LOD,
  deterministic sampling, renderer-driven selection, and payload behavior
  measured rather than assumed.
- Design policy records file-format arguments and layer identity as a design
  principle.
- Roadmap phase table reflects what shipped: phase 2 is complete, and phase 4
  is split into the completed authoring work (4a) and the not-started
  streaming work (4b).
- OpenUSD compatibility statement records the `usdLod` and payload surface now
  in use, and that `metadataOnly` reads are supported rather than refused.

### Fixed

- LAS Extra Bytes validation for non-finite values and for 64-bit integers that
  cannot be represented exactly as `double`.
- Metadata-only attribute reporting and empty-point-cloud handling.
- Payload asset-path portability across a moved output directory.
- Tiled LOD authoring edge cases.
- Aliased point-sampling output.
- Reader range and diagnostic accounting.
- LAS waveform field layout.
- LAS legacy attribute authoring.
- Big-endian LAS decoding.

### Known limitations

- Rendering is provided by the consuming OpenUSD application.
- Peak RSS and payload working-set measurements for real-world tiled datasets
  are not yet published; the generated-corpus benchmark is included in the
  release verification record.
- Bounds and classification filters remain unavailable and are rejected with
  typed diagnostics.
- GeoTIFF keys are parsed and retained but are not interpreted, and EPSG codes
  are not inferred when the WKT CRS VLR is absent.

## [0.1.0] - 2026-08-01

### Added

- Shared geospatial values, transforms, bounds, and point-cloud contracts.
- LAS 1.2-1.4 header, VLR/EVLR, WKT CRS, and point-record support.
- LAZ chunk decoding through the bundled `laz-perf` adapter.
- OpenUSD FileFormat Plugins for LAS and LAZ.
- Machine-readable diagnostics for invalid or unsupported input.
- Apache License 2.0 project licensing and third-party notices.

### Known limitations

- Rendering is provided by the consuming OpenUSD application.
- Tile/LOD streaming and a USDC cache are not included in this release.
- OpenUSD is a required runtime dependency for the USD targets.

[0.1.0]: https://github.com/animu-sphere/usd-pointcloud-plugins/releases/tag/v0.1.0
[0.2.0]: https://github.com/animu-sphere/usd-pointcloud-plugins/releases/tag/v0.2.0
[0.2.1]: https://github.com/animu-sphere/usd-pointcloud-plugins/releases/tag/v0.2.1
[0.2.2]: https://github.com/animu-sphere/usd-pointcloud-plugins/releases/tag/v0.2.2
[0.3.0]: https://github.com/animu-sphere/usd-pointcloud-plugins/releases/tag/v0.3.0
