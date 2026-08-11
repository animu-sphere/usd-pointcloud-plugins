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

## Shipped in `v0.2.0` (2026-08-05)

### Readers and decoding

- [x] Endian-safe binary decoding
- [x] Add typed diagnostics to `usdGeoCore` and LAS / LAZ reader APIs
- [x] Move the LAS / LAZ FileFormat Plugins onto the typed diagnostics path
- [x] GeoTIFF CRS VLR parsing (structured VLR metadata)
- [x] Extra Bytes descriptors and scalar generic point attributes
- [x] Remaining LAS 1.4 point attributes, including NIR
- [x] Waveform contract and LAS point formats 4, 5, 9, and 10
- [x] Chunked and range-based reader API
- [x] LAS pull-based `PointStream` factory and bounded chunk delivery
- [x] LAZ pull-based `PointStream` factory and bounded chunk delivery
- [x] Metadata-only LAS and LAZ reads

### Plugin adapters

- [x] Record the plugin adapter and file-format argument contracts
- [x] Move `pointcloud-las` onto `usdlas::LasReader`
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
- [x] LAS and LAZ stream connection to tiled payload authoring
- [x] Spatial `tile`, `tileSize`, `tileMemoryLimit`, and `payloadDirectory`
      file-format arguments

### Structure and documentation

- [x] Rename `usd-geo-usd` to `usd-pointcloud-authoring`
      (`usdGeoUsd` to `usdPointCloudAuthoring`)
- [x] Rename `plugins/geospatial-las` and `plugins/geospatial-laz` to
      `plugins/pointcloud-las` and `plugins/pointcloud-laz`, including bundle metadata, resource paths, and CI paths
      names, `plugInfo.json` type names, and CI paths
- [x] Reorganize `docs/` by responsibility and add the documentation index
- [x] Promote the library architecture into a binding
      [workspace contract](../architecture/WORKSPACE.md)
- [x] Add a `README.md` for every module under `libs/` and `plugins/`
- [x] Record the rename in [migration](../compatibility/MIGRATION.md)

## Release track status

### `v0.2.1` — implementation stabilization and conversion tooling

This patch release stabilizes the LAS and LAZ implementation released in
`v0.2.0`. It does not add a new point-cloud format.

- [x] Publish real-dataset processing-time, peak-RSS, spool, and payload output
      measurements
- [x] Measure payload working sets through the available Storm scene/view and
      headless render paths
- [x] Complete long-running, cancellation, failure, and interruption cleanup
      validation for tiled reads
- [x] Add the explicit LAS/LAZ conversion tool as the production path for
      tiled, payload-backed generation, including deterministic manifest output
- [x] Close release documentation gaps for compatibility, installation,
      licensing, and large-data operation
- [x] Add regression coverage for each stabilization fix

### `v0.3.0` — COPC read support (shipped)

COPC shipped as local, read-only support using the existing point, streaming,
tiling, diagnostics, and `usdLod` contracts.

- [x] Add the OpenUSD-independent COPC metadata and hierarchy reader foundation
      (LAS 1.4 COPC Info VLR and local hierarchy pages)
- [x] Read selected local COPC point-data byte ranges through the shared LAZ
      chunk decoder
- [x] Add a format-specific COPC reader and thin FileFormat Plugin adapter
- [x] Validate COPC information and hierarchy metadata
- [x] Read required hierarchy nodes and point-data byte ranges selectively
- [x] Map native hierarchy and resolution metadata to the shared tile/LOD model
- [x] Add the COPC pull-based point stream over native hierarchy order
- [x] Add LAS, LAZ, and COPC point-stream equivalence regression coverage
- [x] Verify LAS, LAZ, and COPC equivalence across authored LOD representations
- [x] Keep COPC writing, HTTP range sources, network caching, and new public
      USD schemas out of the local-read release

### `v0.4.0` — PLY read support (shipped)

- [x] Decode scalar PLY vertex properties through the shared PointStream
      contract with bounded source reads
- [x] Support ASCII, binary little-endian, and binary big-endian scalar vertex
      records, including range and filter controls
- [x] Add the thin `pointcloud-ply` FileFormat Plugin with explicit CRS
      arguments and shared authoring
- [x] Connect PLY reads to payload-backed fixed-grid tiled authoring
- [x] Add PLY plugin discovery, stage-open, corpus, and tiled integration tests
- [x] Keep faces, mesh authoring, PLY writing, and metadata-only reads out of
      scope

## Follow-up Status

### Completed: bounded-memory streaming and spatial tiling stabilization

The completed work is documented in [streaming and tiling](streaming-and-tiling.md).

- [x] `PointStream` pull interface in `usdPointCloudCore`
- [x] `usdPointCloudTiling`: fixed-grid tile keys, configuration, and source-coordinate tile router
- [x] Spool schema, thresholds, cleanup, and deterministic iteration order
- [x] Bounded-memory tests and generated large-corpus spill coverage
- [x] Generated-corpus streaming benchmark and documented measurement command
- [x] Full-size real-dataset processing-time, RSS, spool, and payload output
      measurements
- [x] Payload working-set measurements through the available Storm scene/view
      and headless render paths
- [x] Failure and interruption cleanup validation across tiled reads
- [x] Explicit conversion tool with atomic publish and deterministic manifest
      output

The stabilization slice completed a reproducible LAS/LAZ measurement matrix
using the checked-in 4,096-point thinned corpora, full-size Shizuoka LAS and
derived-LAZ measurements, and regression coverage for recovery when a
conversion transaction marker exists without its state file. Full-size
interruption recovery is validated by force-terminating the converter after
transaction state creation and retrying the same output workspace.

### Completed follow-up work

- [x] Extra Bytes descriptor-name normalization contract
- [x] Vector Extra Bytes types
- [x] Bounds and classification filter arguments for LAS and LAZ
- [x] EPSG inference and conflicting-CRS detection
- [x] Declare OST smoke fixtures in both bundle manifests and pass L3/L4
      `usdcat.read` and `python.stage_open`
- [x] Add the initial `usdGeoCache` contracts for deterministic descriptor keys,
      USDC layout, cache lookup, and entry invalidation

### Remaining open work

- [x] Complete PLY scalar vertex decoding and the thin `pointcloud-ply`
      FileFormat adapter with explicit CRS arguments

- [x] Integrate deterministic USDC cache generation and lookup into the
      conversion tool through `--cache-root`
- [x] Integrate cache lookup into direct FileFormat and authoring paths. The
      conversion tool owns generation through `--cache-root`; LAS, LAZ, COPC,
      and PLY adapters read committed entries through `USDGEO_CACHE_ROOT`.
- [x] Stage licensing, notice, capability, compatibility, and installation
      documents into release assets
- [x] Adopt narrow format-specific dynamic LOD fields after generated assets
      and cache lookup stabilized ([ADR 0003](../adr/0003-dynamic-file-format.md))
- [x] Add the project-owned random-access byte source contract and move LAS
      and local COPC reads onto it
- [x] Add resolver-backed `ArAsset` adaptation without introducing OpenUSD or
      transport dependencies into `usdCopc`

### Next release direction

- [x] Complete the v0.3.x documentation consolidation: concise root README
      and synchronized workspace, capability, and implementation documents.
- [x] Complete v0.4.0 PLY point-cloud read support with bounded source
      streaming and tiled authoring follow-up work after the shipped scalar
      direct-read and cache-lookup slice.
- [x] Complete v0.5.0 COPC random access with an OpenUSD `ArAsset` adapter,
      resolver-dependent HTTP support, and conservative generated-cache
      identity; the project-owned source interface and local migration are
      shipped. Resolver-backed cache reuse is disabled when a stable local
      filesystem identity is unavailable.

### Planned infrastructure maturity

The ordered plan and acceptance priorities are in the
[infrastructure maturity roadmap](infrastructure-maturity.md).

#### `v0.6.0` - cache and source identity (shipped)

- [x] Define a format- and transport-independent source identity contract
-     `usdgeo::cache::SourceIdentity` accepts a stable identifier and
      validation token, with optional size and modification metadata;
      local filesystem identity construction is shared by authoring and the
      conversion tool
- [x] Define machine-readable cache lookup states for missing, incomplete,
      hit, and invalid layouts while preserving the `IsCacheHit` API
- [x] Document cache invalidation and compatibility rules in the
      `usdGeoCache` module README
- [x] Add cache statistics and stable diagnostics. `usdgeo::cache` exposes
      stable lookup status names and process-local hit, miss, incomplete, and
      invalid-layout counters through `LookupStatistics`.
- [x] Harden corrupt-entry and interrupted-publication recovery
      committed cache roots are opened and payload references are validated
      before reuse; unreadable roots and invalid or missing payloads are
      invalidated through the descriptor-derived entry path
- [x] Establish local and resolver-backed cache reuse baselines. The
      conversion integration test records the local miss-to-hit path, while
      `pointcloudCopc_integration` verifies that configuring `USDGEO_CACHE_ROOT`
      does not enable reuse for resolver assets without a stable validation
      token.

#### `v0.7.0` - adaptive tiling

- [ ] Define deterministic point-budget planning and limits
- [ ] Preserve fixed-grid `tileSize` and `tileMemoryLimit` behavior
- [ ] Add tile statistics and planning diagnostics
- [ ] Compare LAS, LAZ, COPC, and PLY payload and memory behavior

E57, delimited text, and other point-cloud formats follow these infrastructure
milestones. A public custom USD schema remains deferred until the documented
plain-attribute metadata contract is stable across formats.

## Notes

The sampling contract uses a versioned fixed-stride selection that preserves
source order and applies the same indices to every populated point attribute.
Its algorithm, version, and target count are normalized cache-key inputs.

Argument normalization makes the streaming reader's chunk, point-range, and
tiled payload controls reachable through the plugin layer and the conversion
tool. Bounds and classification filters are normalized at the plugin boundary,
and attribute selection is normalized before authoring; see the
[plugin adapter contract](../architecture/PLUGIN_ADAPTER.md) and the
[file-format argument contract](../architecture/FILE_FORMAT_ARGUMENTS.md).

Compact `lod` profiles author a single non-tiled `usdLod` root through the
shared authoring path for LAS and LAZ. Tiled LAS and LAZ reads now consume
bounded pull-stream chunks, spool points by source-coordinate tile, and author
one payload-backed level per tile. The explicit conversion tool is the
production entry point for long-running generation; static FileFormat tiling
remains available for compatibility, preview, and small inputs. Generated-
corpus bounded-memory measurement, the Shizuoka LAS baseline, and payload
working-set measurements are documented in the [streaming and tiling roadmap](streaming-and-tiling.md).
A broader real-world dataset matrix remains open.

The LAS conformance fixture and FileFormat Plugin integration gate passed
before LAZ integration. The LAZ reader uses the same point-cloud authoring path
and validates chunked decoder output through its own FileFormat Plugin
integration fixture. Documented support is tracked in
[capability matrix](../reference/CAPABILITY_MATRIX.md), which is updated in the
same change as any decoder that widens it.
