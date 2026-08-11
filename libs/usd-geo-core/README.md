# usdGeoCore

## Purpose

`usdGeoCore` is the format-independent, OpenUSD-independent base of the
workspace. Every other module depends on it and it depends on nothing but the
C++ standard library. It exists so that coordinate handling, bounds, CRS
metadata, tile identity, cache keys, and the diagnostic vocabulary have exactly
one definition shared by readers, tiling, and authoring.

CMake package `usdGeoCore`, target `usdgeo::core`, C++ namespace `usdgeo`.

## Responsibilities

- `Vec3d` and finite-value validation.
- `SpatialBounds`: validity, expansion, centre, and size.
- `GeoReference`: EPSG code, WKT, PROJJSON, linear unit, source and stage up
  axis, local origin, and the explicit source ↔ stage-local transforms.
- `TileId`: a deterministic level/x/y/z tile identity with a stable string form.
- Cache-key normalization: `NormalizeCacheArguments` and `StableCacheKey` over
  an ordered key/value list.
- `RandomAccessSource` and `LocalRandomAccessSource`: deterministic source-size
  discovery and bounded offset reads for format readers and resolver adapters.
- The shared diagnostic vocabulary: `Severity`, `DiagnosticCode`, and
  `Diagnostic` with optional byte-offset and point-index anchors.

## Non-responsibilities

- LAS- or LAZ-specific parsing, or any other format.
- Point-cloud storage policy, attribute schemas, or chunking.
- Spatial partitioning policy or tile persistence.
- OpenUSD types, stage authoring, or `usdLod`.
- Plugin registration.
- PROJ, GDAL, or any CRS-transformation engine. EPSG is treated as an
  identifier; WKT or PROJJSON is the authoritative representation.

## Public API

```text
usdgeo/SpatialBounds.h   Vec3d, SpatialBounds
usdgeo/GeoReference.h    GeoReference and its coordinate transforms
usdgeo/TileId.h          TileId
usdgeo/CacheKey.h        CacheArguments, NormalizeCacheArguments, StableCacheKey
usdgeo/Diagnostic.h      Severity, DiagnosticCode, Diagnostic
usdgeo/RandomAccessSource.h  RandomAccessSource, LocalRandomAccessSource
```

Minimal use:

```cpp
#include "usdgeo/GeoReference.h"

usdgeo::GeoReference reference;
reference.sourceUpAxis = "Z";
reference.stageUpAxis = "Y";
reference.localOrigin = {8242.5, 34212.5, -12.4};

usdgeo::Vec3d local;
if (!reference.TryToLocal(sourcePoint, local)) {
    // non-finite input, or an unsupported up-axis pair
}
```

## Dependencies

The C++17 standard library. Nothing else, by contract — see
[WORKSPACE.md §2](../../docs/architecture/WORKSPACE.md). OpenUSD is **not**
required, and no OpenUSD header may enter this module.

## Data flow

`usdGeoCore` is a value layer, not a pipeline stage. Readers construct
`GeoReference` and `SpatialBounds` from source metadata, pass them through
`usdPointCloudCore` contracts, and the authoring library converts them into
stage metadata. Coordinates enter as source-space `double`, and the transform
to stage-local is always explicit — nothing in this module converts
implicitly.

## Error and diagnostic behavior

Fallible operations return `bool` and use out-parameters; source reads report
typed `Diagnostic` values for invalid ranges, missing files, and short reads.
Validity predicates are `noexcept` and total (`IsValid`, `IsFinite`).

## Threading and ownership

All types are plain values with automatic storage. There is no shared mutable
state, no global registry, and no allocator policy. Distinct instances may be
used concurrently without synchronization; a single instance follows the usual
rule that concurrent reads are safe and a concurrent write is not. Returned
values are copies, so no buffer lifetime is inherited from this module.

## Coordinate-space assumptions

`GeoReference` distinguishes source space from stage-local space by name and
converts only through `TryToLocal` / `TryToSource`. `localOrigin` is expressed
in source coordinates and is explicit configuration — it is never chosen
implicitly. Source coordinates stay `double` throughout; narrowing to `float`
happens only in the authoring library, against the local origin. The binding
statement is [ADR 0001](../../docs/adr/0001-coordinate-model.md).

## Build and test

Builds and tests with plain CMake and **no OpenUSD runtime**:

```powershell
ost build
ost test --filter '^usdGeoCore_unit$' --jobs 1
```

In the workspace flow, `ost build` and `ost test` cover it, and
`ost plugin build plugins/pointcloud-las` resolves and builds it before the
bundle. See [BUILDING.md](../../docs/guides/BUILDING.md).

## Known limitations

- Up-axis conversion supports the `Z` source / `Y` stage pair the LAS and LAZ
  readers need. Other pairs are not implemented.
- No CRS transformation: a CRS is carried and reported, never reprojected.
- The generic core does not infer EPSG codes or detect conflicting CRS records;
  format-specific readers, such as `usdLas`, perform that resolution before
  populating `GeoReference`.
- `TileId` is an identity and a string form only. It carries no bounds, no
  grid configuration, and no ordering policy; those belong to the reserved
  `usdPointCloudTiling` module.

## Planned work

- Diagnostic codes for the spool and payload failures introduced by
  [streaming and tiling](../../docs/roadmap/streaming-and-tiling.md).
- Cache-key inputs for tiling configuration, once that configuration exists.
- `usdGeoCache` builds on `StableCacheKey` for USDC cache identity, layout,
  lookup, and invalidation. Generation and manifest publication belong to its
  OpenUSD-aware callers.
