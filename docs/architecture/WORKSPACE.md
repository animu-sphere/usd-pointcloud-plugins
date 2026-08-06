# Workspace contract

This is the binding structural contract for `usd-pointcloud-plugins`. It fixes module
identities, dependency directions, root responsibilities, artifact naming, and
change invariants. A structural change that contradicts this document must
change this document first.

Status: `usdGeoCore`, `usdPointCloudCore`, `usdLas`, `usdLaz`, `usdCopc`, and
`usdPointCloudAuthoring` are implemented, as are the `pointcloud-las`,
`pointcloud-laz`, and `pointcloud-copc` bundles. Future module identities are
reserved here.

## 1. Components

| Identity | Directory | Kind | Status | Responsibility |
| --- | --- | --- | --- | --- |
| `usdGeoCore` | `libs/usd-geo-core` | plain CMake/OpenStrata static library | implemented | Format-independent geospatial values: coordinates and local-origin transforms, spatial bounds, CRS-related values, deterministic tile IDs, normalized cache-key inputs, and the typed diagnostic vocabulary shared across modules. |
| `usdPointCloudCore` | `libs/usd-pointcloud-core` | plain CMake/OpenStrata static library | implemented | Format-independent point-cloud contracts: point attribute definitions and types, `PointChunk` / `PointData` / `PointCloudAsset`, point validation, attribute selection, range and chunk read options, deterministic sampling, and the shared LOD hierarchy and tile value types. |
| `usdLas` | `libs/usd-las` | plain CMake/OpenStrata static library | implemented | LAS parsing and decoding: 1.2-1.4 headers, VLR/EVLR, point formats 0-10, waveform packet metadata, GeoTIFF key records, scalar and vector Extra Bytes, metadata-only reads, and chunked and range-based reads behind `LasReader`. |
| `usdLaz` | `libs/usd-laz` | plain CMake/OpenStrata static library | implemented | LAZ decompression through the vendored laz-perf codec, converted into the shared LAS and point-cloud contracts, with metadata-only reads, chunked reads, and stable diagnostics for unsupported compressed formats. Owns the vendored laz-perf subset. |
| `usdPointCloudAuthoring` | `libs/usd-pointcloud-authoring` | plain CMake/OpenStrata static library | implemented | Shared OpenUSD authoring: `UsdGeomPoints` and geospatial metadata authoring, point-attribute authoring, `usdLod` hierarchy authoring, tiled point-cloud authoring, payload-backed tile assets, root layer generation, and stage/layer validation. Renamed from `usdPointCloudAuthoring` after v0.1.0. |
| `pointcloud-las` | `plugins/pointcloud-las` | OpenStrata plugin bundle (`usd-fileformat`) | implemented | OpenUSD `SdfFileFormat` adapter for `.las`: plugin registration, argument normalization, `LasReader` construction, and authoring through the shared library. Owns its `LASxxx` diagnostic codes. |
| `pointcloud-laz` | `plugins/pointcloud-laz` | OpenStrata plugin bundle (`usd-fileformat`) | implemented | The same adapter shape for `.laz`, using `LazReader` and the laz-perf codec integration. Owns its `LAZxxx` diagnostic codes. |
| `usdCopc` | `libs/usd-copc` | plain library | foundation implemented | COPC Info and hierarchy validation plus local point-data range selection through the shared LAZ chunk decoder. |
| `pointcloud-copc` | `plugins/pointcloud-copc` | OpenStrata plugin bundle (`usd-fileformat`) | implemented (local read) | Local metadata-only, non-tiled, and native hierarchy tiled COPC adapter. Tiled output uses payload-backed shared `usdLod` authoring; source point ranges remain unsupported. |
| `usdPointCloudTiling` | `libs/usd-pointcloud-tiling` | plain library | implemented (initial contracts) | Format-independent spatial partitioning and bounded-memory tile preparation: fixed-grid tile keys and source-coordinate bucketing are implemented; tile buffering and spill-to-disk, tile manifests, fixed-stride LOD sampling, deterministic tile and level ordering, and cleanup of incomplete temporary output remain. See the [streaming and tiling plan](../roadmap/streaming-and-tiling.md). |
| `usdGeoCache` | `libs/usd-geo-cache` | plain library | reserved, not implemented | Stable cache keys, USDC tile layout, cache lookup and invalidation. |
| `usdPly`, `usdAsciiPoints`, `usdE57` | `libs/` | plain libraries | reserved, not implemented | Additional point-cloud readers targeting the same shared contracts. |
| `geospatial-ply`, `geospatial-points-text`, `geospatial-e57` | `plugins/` | reserved, not implemented | Future point-cloud FileFormat Plugin adapters, in the order fixed by [format support order](../roadmap/format-support-order.md). |

Terrain, raster, and vector contracts belong to future repository candidates:
`usd-terrain-plugins` and `usd-vector-plugins`. They are not reserved modules
or active roadmap phases in this repository.

None of the `libs/` modules is a plugin: none has a `plugInfo.json`, none
performs plugin registration, and only `usdPointCloudAuthoring` exposes OpenUSD
types. Only create a reserved directory when its first tested capability is
implemented; the table describes ownership boundaries, not a requirement to
scaffold empty modules.

`.csv` and `.json` are generic extensions. The bundles that would claim them
never do so by default; a host opts in through an explicit file-format
selection, and the readers require the arguments they need instead of
inferring a layout.

## 2. Dependency directions

Allowed today:

```text
usdPointCloudCore      -> usdGeoCore
usdLas                 -> usdGeoCore, usdPointCloudCore
usdLaz                 -> usdLas, usdPointCloudCore
usdCopc                -> usdLas, usdLaz, usdPointCloudCore
usdLaz                 -> laz-perf (private, vendored codec implementation)
usdPointCloudAuthoring -> usdGeoCore, usdPointCloudCore
usdPointCloudAuthoring -> OpenUSD (usdGeom, usdLod)
pointcloud-las         -> usdLas, usdPointCloudAuthoring, OpenUSD
pointcloud-laz         -> usdLaz, usdPointCloudAuthoring, OpenUSD
pointcloud-copc        -> usdCopc, usdPointCloudAuthoring, OpenUSD
```

Reserved future directions:

```text
usdPointCloudTiling    -> usdGeoCore, usdPointCloudCore
pointcloud-las         -> usdPointCloudTiling
pointcloud-laz         -> usdPointCloudTiling
any format reader      -> usdGeoCore, usdPointCloudCore
any format bundle      -> usdPointCloudAuthoring
```

Forbidden:

```text
usdGeoCore             -> anything (OpenUSD, a reader, a codec, PROJ, GDAL)
usdPointCloudCore      -> OpenUSD, LAS/LAZ, or usdLod schema types
usdLas or usdLaz       -> OpenUSD, tiling policy, payload generation,
                          plugin registration
usdPointCloudTiling    -> LAS/LAZ parsing, OpenUSD, plugin registration
usdPointCloudAuthoring -> LAS/LAZ decoding, spatial partitioning policy,
                          plugin argument parsing, renderer implementation
any plugin bundle      -> another plugin bundle
any dependency cycle
```

The readers must not depend on the tiling or OpenUSD authoring modules, and the
tiling module must not depend on LAS or LAZ. The plugin adapters are what
compose readers, tiling, and OpenUSD authoring.

Each `libs/*/openstrata.library.yaml` gives the plain library a workspace
identity and CMake package/target. A bundle declares the edge in its manifest;
`ost plugin build/test/package` resolves and executes it.

| Identity | CMake package | CMake target |
| --- | --- | --- |
| `usdGeoCore` | `usdGeoCore` | `usdgeo::core` |
| `usdPointCloudCore` | `usdPointCloudCore` | `usdpointcloud::core` |
| `usdPointCloudAuthoring` | `usdPointCloudAuthoring` | `usdpointcloud::authoring` |
| `usdPointCloudTiling` | `usdPointCloudTiling` | `usdpointcloud::tiling` |
| `usdLas` | `usdLas` | `usdlas::core` |
| `usdLaz` | `usdLaz` | `usdlaz::core` |

`usdpointcloud::authoring` remains as a deprecated alias of `usdpointcloud::authoring` and
is removed in v0.3.0; see [MIGRATION.md](../compatibility/MIGRATION.md).

## 3. Source boundaries

```text
plugins/pointcloud-las/src/UsdGeoLasFileFormat.cpp
    thin SdfFileFormat integration: argument normalization, reader call,
    authoring call, diagnostic projection

plugins/pointcloud-las/include/usdgeolas/UsdGeoLasDiagnostics.h
    the bundle's stable LASxxx codes

plugins/pointcloud-laz/src/UsdGeoLazFileFormat.cpp
    the same integration for .laz

plugins/pointcloud-laz/include/usdgeolaz/UsdGeoLazDiagnostics.h
    the bundle's stable LAZxxx codes

libs/usd-las/
    LAS header, VLR/EVLR, point-record decoding, attribute fan-out,
    GeoReference and bounds construction, LasReader orchestration

libs/usd-laz/
    laz-perf isolation and the equivalent LazReader orchestration

libs/usd-pointcloud-core/
    the point schema, chunk contracts, read options, sampling, LOD values
    every reader and the authoring library share

libs/usd-geo-core/
    format- and USD-independent geospatial values and diagnostics

libs/usd-pointcloud-authoring/
    OpenUSD authoring, shared by every format bundle
```

The C++ namespaces are `usdgeo`, `usdpointcloud`, `usdlas`, `usdlaz`, and the
per-bundle `usdgeolas` / `usdgeolaz`. Directory names, CMake target names, and
C++ namespaces are deliberately not required to be identical: the external
bundle name uses the explicit `geospatial` term while the internal C++ prefix
stays `UsdGeo` to avoid excessively long symbols. The authoring library keeps
the `usdgeo` namespace and the `include/usdgeo/` header path it had as
`usdPointCloudAuthoring`.

Plugin C++ sources, `plugInfo.json`, format fixtures, and the per-bundle
diagnostic tables belong to the bundle. Each bundle must remain buildable
through `ost` as an independent bundle; the root CMake build is an additional
supported path.

## 4. Root responsibilities

The repository root owns composition, not module implementation:

- the plain CMake build that wires `libs/` and `plugins/` together;
- workspace-wide version and OpenStrata platform/profile selection;
- `openstrata.ci.yaml` and generated CI;
- shared licensing, third-party notices, documentation, and release records;
- cross-format equivalence tests and aggregate packaging.

## 5. Authored stage contract

Every read using the shared authoring contract produces, for `lod=off`:

```text
/PointCloud             UsdGeomPoints
```

The stage is Y-up with one meter per unit. Positions are stage-local `float`
values relative to `geo:localOrigin`; source coordinates are recovered through
that origin. Compact LOD profiles author a single non-tiled `usdLod` root at
the same path. The tiled and payload-backed shapes the authoring library can
also produce, and the exact metadata authored on each prim, are fixed in
[LOD.md](LOD.md) and
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md).

Spatial partitioning uses source horizontal coordinates while OpenUSD
authoring uses stage-local coordinates. This keeps tile identity stable across
stage up-axis conversion and preserves the meaning of projected CRS
coordinates. See [ADR 0001](../adr/0001-coordinate-model.md).

## 6. Artifact naming and versioning

Per-bundle artifacts use OpenStrata's target-qualified convention:

```text
pointcloud-las-<version>-<target>.tar.zst
pointcloud-laz-<version>-<target>.tar.zst
```

The installed shared libraries are `UsdGeoLasFileFormat` and
`UsdGeoLazFileFormat`, and the registered `plugInfo.json` type names match
them. Until a real need for independent release cadences appears, every bundle
and plain library mirrors the repository-root `VERSION`. Git tags use `vX.Y.Z`.

## 7. Change invariants

Every structural or format change preserves these invariants:

1. Public APIs use project-owned value types and standard-library types.
2. Source coordinates and stage-local coordinates use distinct names and
   explicit transforms.
3. Readers return deterministic, validated intermediate data and never author
   USD directly.
4. File-format arguments are normalized before reader or cache lookup, and
   normalized arguments participate in layer identity. See
   [FILE_FORMAT_ARGUMENTS.md](FILE_FORMAT_ARGUMENTS.md).
5. Unknown source metadata is preserved or reported; it is never silently
   discarded.
6. Tile, LOD, and cache contracts are shared, while format-specific spatial
   indexes remain private.
7. Spatial tiling and level of detail stay separate concepts; neither collapses
   into the other.
8. `UsdLodRootAPI`, Hydra, cameras, viewport state, and screen-space math never
   appear in a reader or a plugin adapter. See [LOD.md](LOD.md).
9. A new format reaches USD through `PointCloudAsset` and the shared authoring
   entry point, not by copying LAS assumptions into a new writer.
10. Manifest and CMake dependency declarations change together.
11. Plugin registration changes include a discovery test.
12. Third-party revision and license changes update both notices and package
    verification.
13. A change that modifies a module contract updates that module's `README.md`
    in the same pull request. See
    [MODULE_README_CONTRACT.md](../contributing/MODULE_README_CONTRACT.md).
14. Write support is deferred until read behavior and preservation rules are
    stable.

## 8. Build and packaging

- Use OpenStrata manifests for libraries and plugin bundles.
- Keep the root CMake build working without `ost` for local development.
- Keep large dependencies optional and scoped to the owning target.
- Test pure libraries without requiring an OpenUSD runtime where possible.
  `usdGeoCore`, `usdPointCloudCore`, `usdLas`, and `usdLaz` build and test with
  no OpenUSD runtime; `usdPointCloudAuthoring` and both bundles require one.
- Validate plugin bundles with the pinned OpenStrata `cy2026` / `usd` runtime.

## 9. CI and verification contract

`openstrata.ci.yaml` is the source of truth; the GitHub workflow is generated
by `ost ci generate github`. The declared PR matrix runs both bundles on every
host:

| Host | Target | OST level |
| --- | --- | --- |
| Windows 2022 x86_64 | cy2026 / USD | L0-L4 |
| macOS 15 arm64 | cy2026 / USD | L0-L5 |
| Ubuntu 24.04 x86_64 | cy2026 / USD | L0-L5 |

The required local gate is:

```text
ost configure
ost build
ost test
ost plugin build plugins/pointcloud-las
ost plugin build plugins/pointcloud-laz
ost plugin test plugins/pointcloud-las --up-to 4
ost plugin test plugins/pointcloud-laz --up-to 4
```

Neither bundle declares OST test fixtures yet, so the L3 `usdcat.read` and L4
`python.stage_open` checks currently skip. Wiring the existing conformance
fixtures into the manifests is open work tracked in
[roadmap/implementation-status.md](../roadmap/implementation-status.md).

## 10. Delivery status

| Milestone | Boundary | Status |
| --- | --- | --- |
| v0.1.0 | shared geospatial and point-cloud contracts, LAS and LAZ readers, FileFormat Plugin integration | released 2026-08-01 |
| unreleased | typed diagnostics end to end, LAS 1.4 attributes and waveform formats, GeoTIFF key parsing, scalar and vector Extra Bytes, chunked and range-based reads, file-format arguments, shared authoring entry point, `usdLod` authoring, compact LOD profiles, tiled and payload-backed authoring, metadata-only reads, and the module rename | on `main` |
| next | bounded-memory streaming: `PointStream`, `usdPointCloudTiling`, spill-backed tile routing, payload generation during file open, spatial tile arguments | in progress; generated-corpus benchmark is available, while real-dataset measurement remains in [streaming and tiling](../roadmap/streaming-and-tiling.md) |

Current work and acceptance gaps are tracked in
[roadmap/implementation-status.md](../roadmap/implementation-status.md).
