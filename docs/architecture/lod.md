# Tile and LOD Contract

Section 5 of the [development policy](../development-policy.md) requires a
shared tile contract and an OpenUSD level-of-detail representation. This
document records the binding decision, the target USD structure, and the
invariants the shared layer enforces.

Status: **shared contracts implemented**. The format-independent tile and LOD
value types and validation live in `usdPointCloudCore`; USD authoring is not
implemented yet. This document defines the contract that Phase 4 work is
measured against; see the [implementation status](../roadmap/implementation-status.md).

## 1. Standing Policy

> OpenUSD 26.08 `usdLod` is the only public LOD representation authored by the
> project. Spatial tiling, point sampling, source-range access, caching, and
> payload packaging are internal or compositional mechanisms that support that
> representation. LOD selection remains the responsibility of the consuming
> OpenUSD application or renderer.

## 2. Public Representation

The authored stage uses the schemas provided by OpenUSD 26.08:

| Schema | Role |
| --- | --- |
| `UsdLodRootAPI` | Applied to a prim whose direct children are interchangeable LOD items |
| `UsdLodScreenSizeHeuristic` | Advises a renderer which child to select |
| `UsdLodOverrideAPI` | Forces or changes selection for debugging and final-quality renders |

The repository never defines a competing public LOD representation. The
following are all out of scope:

- a repository-specific schema such as `GeoLodRoot` or `PointCloudLodAPI`;
- a custom `lodLevel` primvar;
- a variant-set-based LOD convention;
- format-specific LOD attributes.

Internal C++ structures may model LOD generation freely. Only the authored USD
stage is constrained.

The exact property names and schema API calls follow the `usdLod` schema
definitions in the OpenUSD build being compiled against. Where this document
shows authored properties, they are illustrative until verified against the
pinned 26.08 runtime; see
[phase 0](../roadmap/phase-0-technical-validation.md).

## 3. Tile and LOD Are Separate

| Concept | Responsibility |
| --- | --- |
| Tile | Spatial partition, bounds, source range, payload or cache unit |
| LOD item | One representation of a tile at a particular point density |
| LOD root | Groups interchangeable LOD items for one tile |
| Heuristic | Advises a renderer which LOD item to select |
| Override | Forces or changes LOD selection |

A tile is a spatial and loading unit. An LOD item is one resolution of the same
spatial region. The two are never collapsed into a single ambiguous index, in
either the internal contracts or the authored namespace.

## 4. Target Namespace

Each spatial tile is normally one LOD root.

```text
/PointCloud
    /LodHeuristics
        /ScreenSize
    /Tiles
        /Tile_0_0
            /LOD0
            /LOD1
            /LOD2
        /Tile_0_1
            /LOD0
            /LOD1
            /LOD2
```

Direct children of an LOD root are ordered from highest detail to lowest
detail: `LOD0` is the highest point density, and the last child is the lowest.

```usda
#usda 1.0

def Xform "PointCloud"
{
    def Scope "LodHeuristics"
    {
        def LodScreenSizeHeuristic "ScreenSize"
        {
            uniform token lod:domain = "imaging"
            uniform float[] thresholds = [0.25, 0.10]
        }
    }

    def Scope "Tiles"
    {
        def Xform "Tile_0_0" (
            prepend apiSchemas = ["LodRootAPI"]
        )
        {
            uniform int lod:default:index = 1
            rel lod:heuristics = </PointCloud/LodHeuristics/ScreenSize>

            def Points "LOD0"
            {
                point3f[] points = [...]
            }

            def Points "LOD1"
            {
                point3f[] points = [...]
            }

            def Points "LOD2"
            {
                point3f[] points = [...]
            }
        }
    }
}
```

A reusable heuristic is authored once and referenced by every tile root that
shares it, rather than duplicated per tile.

## 5. Renderer-Driven Selection

FileFormat Plugins do not select an LOD.

The plugin is responsible for reading source data, partitioning or sampling the
point cloud, constructing LOD items, authoring the LOD hierarchy and
heuristics, and reporting diagnostics. The consuming OpenUSD application, scene
delegate, or render delegate selects the active LOD.

Neither the readers nor the plugins may contain camera-distance calculations,
viewport-size calculations, LOD selection state, or renderer-specific
branching.

## 6. Payload and Layering

LOD selection and payload loading are related but distinct. `usdLod` defines
LOD organization and selection intent. It does not by itself guarantee that
non-selected payloads are unloaded or never composed.

The scalable target places each LOD child behind a payload:

```usda
def Xform "Tile_0_0" (
    prepend apiSchemas = ["LodRootAPI"]
)
{
    rel lod:heuristics = </PointCloud/LodHeuristics/ScreenSize>
    uniform int lod:default:index = 2

    def Xform "LOD0" (
        prepend payload = @tiles/tile_0_0_lod0.usdc@
    )
    {
    }

    def Xform "LOD1" (
        prepend payload = @tiles/tile_0_0_lod1.usdc@
    )
    {
    }

    def Xform "LOD2" (
        prepend payload = @tiles/tile_0_0_lod2.usdc@
    )
    {
    }
}
```

This layout is a target, not an assumption. Before any working-set reduction is
claimed, integration tests must measure:

- whether non-selected LOD children are populated by the active scene delegate;
- whether payloads beneath non-selected LOD children are loaded;
- whether renderer selection occurs early enough to reduce memory;
- how Storm and other Hydra render delegates behave;
- how stage population differs from render visibility.

Documentation must keep visibility selection and payload loading distinct. A
test must never report memory savings on the grounds that non-selected children
are invisible.

## 7. Shared Contracts

LOD generation data lives in the format-independent layer. The proposed shapes
are:

```cpp
struct PointTileId {
    std::uint32_t level = 0;
    std::uint64_t mortonCode = 0;
};

struct PointSourceRange {
    std::uint64_t firstPoint = 0;
    std::uint64_t pointCount = 0;
};

struct PointLodItem {
    std::uint32_t index = 0;
    std::uint64_t pointCount = 0;
    usdgeo::SpatialBounds bounds;
    PointSourceRange sourceRange;
};

struct PointLodHierarchy {
    usdgeo::SpatialBounds bounds;
    std::vector<PointLodItem> items;
    std::uint32_t defaultIndex = 0;
    std::vector<float> screenSizeThresholds;
};

struct PointTile {
    PointTileId id;
    usdgeo::SpatialBounds bounds;
    std::vector<PointTileId> children;
    PointLodHierarchy lod;
};
```

`PointSourceRange` is the same selection concept the streaming reader already
exposes as `PointReadOptions::range`; see
[point reader architecture](point-reader.md). LOD generation consumes the
existing chunked reader instead of adding a second source-access path.

`TfToken`, `SdfPath`, `UsdPrim`, and every other OpenUSD type stay out of these
contracts. They appear only in contracts owned by `usdGeoUsd`.

### 7.1 Tile Identity Reconciliation

`usdGeoCore` already publishes `usdgeo::TileId` as `level`, `x`, `y`, `z`.
`PointTileId` uses a Morton code over the same lattice. These must not become
two independent spatial identities: the Morton encoding is defined as a
deterministic, reversible function of the existing `TileId` axes, and one of
the two forms is retained as the canonical identity when the contract lands.
Resolving this is part of Phase 1 of the
[implementation sequence](#12-implementation-sequence).

## 8. Validation Invariants

The shared validation layer enforces:

```text
items are ordered from highest detail to lowest detail
defaultIndex is within the item range
threshold count equals item count minus one
thresholds are finite
thresholds are strictly descending
thresholds are within the supported screen-size domain
all LOD items describe the same spatial region
point counts do not increase as detail decreases
tile IDs are deterministic
tile bounds are finite and valid
```

The fundamental count relation is:

```cpp
hierarchy.items.size() ==
    hierarchy.screenSizeThresholds.size() + 1;
```

Minor bounds differences caused by sampling may be tolerated internally, but
the authored LOD root exposes one stable tile extent so screen-size evaluation
stays consistent across items.

Violations produce typed diagnostics, not exceptions or assertions; see the
[diagnostics contract](diagnostics.md).

## 9. Sampling

LOD generation is independent from the USD authoring layer. Candidate methods
are fixed-stride sampling, deterministic hash sampling, voxel-grid sampling,
and spatially balanced reservoir sampling.

The production method must provide:

- deterministic output for the same source and options;
- stable point ordering where practical;
- bounded memory usage;
- preserved tile bounds;
- reasonable retention of classification and color distribution;
- no camera dependency;
- no renderer dependency.

Random sampling without a stable seed is not used.

The sampling algorithm and its version participate in cache keys, alongside:

```text
source identity
source modification state or content digest
tile identifier
LOD index
target point count
sampling algorithm
sampling algorithm version
selected attributes
coordinate transform
local origin policy
up-axis conversion
```

These extend the cache key inputs in section 7 of the
[development policy](../development-policy.md).

## 10. `usdGeoUsd` Authoring

`libs/usd-geo-usd` owns the mapping from the shared contracts to the OpenUSD
26.08 schemas. The proposed interface is:

```cpp
struct LodAuthoringOptions {
    std::uint32_t defaultIndex = 0;
    std::vector<float> screenSizeThresholds;
    std::string domain = "imaging";
};

bool AuthorPointCloudLod(
    const UsdPrim& root,
    const PointLodHierarchy& hierarchy,
    const LodAuthoringOptions& options,
    std::vector<usdgeo::Diagnostic>& diagnostics);
```

The implementation:

1. applies `UsdLodRootAPI` to the root prim;
2. authors the default LOD index;
3. defines or references a `UsdLodScreenSizeHeuristic`;
4. authors descending thresholds;
5. creates one direct child per LOD item;
6. preserves deterministic child ordering;
7. authors each point representation through the existing point-cloud
   authoring path;
8. validates the authored hierarchy;
9. produces typed diagnostics for invalid contracts.

## 11. Plugin Surface

### 11.1 Read Flow

The LAS and LAZ plugins stay thin adapters:

```cpp
bool GeoLasFileFormat::Read(
    SdfLayer* layer,
    const std::string& resolvedPath,
    bool metadataOnly) const
{
    const auto request =
        usdlas::MakeReadRequest(resolvedPath, metadataOnly);

    auto result = usdlas::ReadPointCloud(request);
    if (!result) {
        ReportDiagnostics(result.diagnostics);
        return false;
    }

    return usdgeo::AuthorPointCloudAsset(
        layer,
        result.metadata,
        result.tiles,
        result.diagnostics);
}
```

A plugin never carries an independent LOD naming convention or a
format-specific USD schema definition.

Neither plugin has this shape today, and the migration onto a shared reader
contract and a single authoring entry point is a prerequisite for LOD work
rather than cleanup afterwards. See the
[plugin adapter contract](plugin-adapter.md).

### 11.2 File-Format Arguments

Arguments may control LOD generation, but never replace the standard USD LOD
representation. The general rules, syntax, and normalization requirements are
in the [file-format argument contract](file-format-arguments.md). The
LOD-specific candidates are:

```text
lod
lodLevels
lodPointCounts
lodRatios
lodThresholds
tile
tileDepth
tilePointLimit
sampling
attributes
classification
bounds
originMode
upAxis
```

Requirements:

- every argument is validated;
- defaults are deterministic;
- normalized arguments participate in layer identity and cache keys;
- unsupported combinations produce stable diagnostics;
- an argument that alters authored topology never reuses an incompatible
  cached layer.

The first version may expose a compact profile instead of the full numeric
surface:

```text
lod=off
lod=preview
lod=balanced
lod=quality
```

Explicit numeric controls are added after the contract stabilizes.

### 11.3 Overrides

`UsdLodOverrideAPI` is supported for debugging, validation, offline rendering,
and quality control: forcing the highest-detail child, forcing a particular
index, disabling heuristic-driven selection in tests, comparing LOD outputs,
capturing deterministic reference images, and producing final-quality renders.

Overrides are authored outside the source asset where possible, in a session
layer or a stronger referencing layer. The LAS or LAZ representation stays
reusable and never permanently forces a renderer-specific quality level.

### 11.4 Metadata-Only Reads

Metadata-only support lands before large-scale tile generation is considered
complete. A metadata-only read avoids decoding point records while still
exposing source point count, source bounds, CRS, scale and offset, point
format, available attributes, tile planning metadata where available, and the
default LOD policy.

The metadata-only stage keeps a namespace compatible with the full read
wherever possible, so asset browsers, stage inspection, tile planning, cache
lookup, preflight validation, and deferred generation all work from it.

## 12. Implementation Sequence

| Phase | Scope |
| --- | --- |
| 1 | `PointTileId`, `PointLodItem`, `PointLodHierarchy`, validation, typed diagnostics, deterministic sampling contracts, LOD cache-key inputs |
| 2 | `usdLod` dependency in `usdGeoUsd`, `UsdLodRootAPI` and `UsdLodScreenSizeHeuristic` authoring, reusable heuristic prims, `UsdLodOverrideAPI` tests, save-and-reopen tests |
| 3 | Remaining read orchestration out of the plugins, deterministic LOD items from the chunked readers, a single non-tiled LOD root, file-format arguments, LAS/LAZ equivalence |
| 4 | Deterministic spatial partitioning, tile hierarchy separated from LOD hierarchy, one LOD root per tile, payload or sublayer packaging, USDC cache generation, stage population and memory measurement |
| 5 | COPC hierarchy nodes onto the shared tile contracts, reuse of source hierarchy and ranges, COPC resolutions onto LOD items, the same public `usdLod` representation, remote and partial-read policies |

## 13. Testing

### 13.1 Contract Tests

Non-USD libraries test deterministic tile IDs, deterministic sampling,
descending point counts, threshold validation, invalid default indices,
inconsistent item bounds, range overflow, empty hierarchies, and cache-key
stability.

### 13.2 Authoring Tests

`usdGeoUsd` tests `UsdLodRootAPI` application, heuristic relationship targets,
screen-size threshold authoring, child ordering, default index authoring,
round-trip through save and reopen, schema validation, and diagnostics for
malformed hierarchies.

### 13.3 Plugin Integration Tests

LAS and LAZ plugin tests verify direct opening through OpenUSD, identical
hierarchy shape for equivalent LAS and LAZ data, correct LOD child count, a
stable namespace, preserved metadata, correct attribute subsets, deterministic
output, and metadata-only behavior once implemented.

### 13.4 Imaging Tests

OpenUSD 26.08 integration tests verify that Storm recognizes the authored LOD
hierarchy, that screen-size thresholds change the selected child, that
overrides force the expected child, that default selection works without a
usable heuristic, that non-selected children are not rendered, and that payload
and working-set behavior are measured separately.

## 14. Compatibility

The LOD implementation targets OpenUSD 26.08 or newer.

```text
OpenUSD < 26.08:
    LAS and LAZ reader libraries may still build where practical.
    Standard LOD authoring is unavailable.

OpenUSD >= 26.08:
    usdLod-based authoring is enabled.
```

No parallel fallback LOD representation is maintained for older runtimes unless
a concrete downstream requirement is accepted. If compile-time guards become
necessary, they isolate the LOD authoring layer; they never produce two public
stage representations. See
[OpenUSD compatibility](../compatibility/openusd.md).

## 15. Non-Goals

The initial LOD implementation does not:

- implement a renderer;
- modify Hydra LOD selection behavior;
- add camera logic to FileFormat Plugins;
- guarantee payload unloading without integration evidence;
- write LAS, LAZ, or COPC;
- expose a repository-specific public LOD schema;
- use LOD variants as the primary mechanism;
- combine spatial hierarchy and resolution hierarchy into one ambiguous index;
- promise out-of-core rendering before working-set behavior is verified.

## 16. Definition of Done

The first LOD milestone is complete when:

- a deterministic multi-resolution point cloud can be generated from LAS;
- the same path works for LAZ;
- the authored stage uses OpenUSD 26.08 `usdLod`;
- each LOD root has correctly ordered direct children;
- a screen-size heuristic is authored and referenced;
- a default index is authored;
- an override can force a selected LOD;
- the stage survives save and reopen;
- Storm integration behavior is covered by tests;
- documentation distinguishes visibility selection from payload loading;
- no camera or viewport logic exists in the readers or the FileFormat Plugins.
