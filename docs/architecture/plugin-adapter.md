# FileFormat Plugin Adapter Contract

Section 3.1 of the [development policy](../development-policy.md) requires that
FileFormat Plugins remain thin adapters, and section 8 of the LOD policy
requires read orchestration to move out of them before tiling and LOD work
begins. This document records the current state, the target shape, and the
migration.

## Rule

A FileFormat Plugin normalizes file-format arguments, invokes a reader, hands
the result to `usdGeoUsd`, and converts diagnostics. It owns nothing else.

Specifically, a plugin must not contain:

- file access, seeking, or byte-range slicing;
- header, VLR, or EVLR interpretation;
- overflow or truncation validation of source offsets and counts;
- per-record decode loops;
- point-attribute fan-out into authoring buffers;
- attribute-schema construction from a point format;
- `GeoReference` or bounds construction from source metadata;
- camera-distance or viewport-size calculations;
- LOD selection state or LOD naming conventions;
- renderer-specific branching;
- format-specific USD schema definitions.

Everything above is either reader behavior or `usdGeoUsd` behavior. Placing it
in a plugin duplicates it across every future bundle and puts it outside the
tests that run without an OpenUSD runtime.

## Current State

`geo-las` now delegates file access and LAS decoding to `usdlas::LasReader`,
and both plugins normalize the supported file-format arguments before calling
their shared readers and authoring path. Metadata-only reads use the same
header/VLR contracts without decoding point records.

| Concern | `geo-las` | `geo-laz` |
| --- | --- | --- |
| Source file access | In `usdlas::LasReader` | In `usdLaz` |
| Header and VLR/EVLR reading | In `usdlas::LasReader` | In `usdLaz` |
| Point-data truncation checks | In `usdlas::LasReader` | In `usdLaz` |
| Per-record decode loop | In `usdlas::LasReader` | In `usdLaz` |
| Uses the chunked reader API | Yes | Yes |
| Attribute fan-out into point data | In `usdlas` | In `usdlas` |
| `PointChunk` attribute schema | In `usdPointCloudCore` | In `usdPointCloudCore` |
| `GeoReference` and bounds construction | In `usdlas` | In `usdlas` |
| Stage creation, metrics, authoring, transfer | In `usdGeoUsd` | In `usdGeoUsd` |
| `metadataOnly` | Header/VLR inspection | Decoder header inspection |

`GeoLasFileFormat::Read` now constructs a `usdlas::LasReader`, consumes its
point chunks, and projects reader diagnostics onto the stable plugin codes.
It passes the validated point-cloud asset to the shared authoring entry point.

`usdlas::LasReader` already provides exactly that orchestration behind
`LasReadOptions`, a chunk consumer, and typed diagnostics. It is currently
called from `geo-las` and its unit tests. `GeoLasFileFormat::Read` now uses it
the same way that `GeoLazFileFormat::Read` uses `usdlaz::LazReader`.

Both plugins now pass reader output through the shared `usdlas` point-data and
asset builders, then call the layer-level `usdgeo::AuthorPointCloudAsset`
entry point. The plugins no longer own point fan-out, chunk schema
construction, CRS or bounds conversion, stage metrics, or layer transfer.

### Consequence

`chunkPointLimit`, `memoryBudgetBytes`, and `range` are now reachable through
the normalized argument request. `isCancelled` remains host-supplied and is
not an asset argument. The whole point cloud is still accumulated for the
current `UsdGeomPoints` authoring path, so this migration makes bounded reader
delivery available without claiming bounded final-layer memory.

The static adapters consume `SdfLayer::GetFileFormatArguments()`, which is the
argument map OpenUSD stores after layer lookup. Callers that construct a layer
must normalize arguments first and pass the request's canonical map to
`SdfLayer::FindOrOpen`; `Read` is too late to change the layer cache key. Pcp
dynamic file-format composition is intentionally deferred to ADR-0003.

## Target Contract

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

Two new pieces carry the work the plugins hold today:

- a reader entry point per format that takes a normalized request and returns
  validated metadata plus tiles, with typed diagnostics;
- `usdgeo::AuthorPointCloudAsset`, which owns the attribute fan-out, the
  attribute schema, `GeoReference` and bounds, stage metrics, prim authoring,
  and later the `usdLod` hierarchy.

`AuthorPointCloudAsset` is the single authoring entry point every present and
future bundle shares. When LOD lands, tiles arrive through the same `result`
and the plugin code above does not change; see the
[tile and LOD contract](lod.md).

The plugin keeps only argument normalization, the reader call, the authoring
call, and the projection of typed diagnostics onto its stable `LASxxx` /
`LAZxxx` prefixes. See the
[diagnostics contract](diagnostics.md).

## Migration

1. [x] Move `geo-las` onto `usdlas::LasReader`, deleting the plugin's file
  access, metadata reading, truncation checks, and decode loop. The LAZ plugin
  already shows the shape.
2. [x] Move the shared tail into the reader and authoring libraries:
  attribute fan-out and LAS metadata conversion live in `usdlas`, chunk
  schema construction lives in `usdPointCloudCore`, and stage metrics and
  layer transfer live in `usdgeo::AuthorPointCloudAsset`. Both plugins call
  the shared path.
3. [x] Normalize file-format arguments in the plugin and pass supported read
  options to the reader. `isCancelled` remains host-supplied and is not part of
  the layer identifier.
4. Add the request/result entry points (`MakeReadRequest`, `ReadPointCloud`) so
   the plugin body reduces to the target contract above.
5. Implement `metadataOnly` through the same request, returning metadata
   without decoding point records.
6. Author LOD through the same authoring call once the LOD contracts land.

Steps 1 and 2 remove the duplication that would otherwise be copied into every
new bundle in the
[library architecture](../roadmap/library-architecture.md). They are
prerequisites for tiling and LOD work, not cleanup to be done afterwards.

## Tests

Plugin integration tests verify authored output, not orchestration. As
behavior moves into the readers and `usdGeoUsd`, coverage moves with it into
tests that run without an OpenUSD runtime.

The invariant the plugin tests keep enforcing is that LAS and LAZ produce the
same layer shape for equivalent data. That equivalence becomes structural once
both call one reader contract and one authoring entry point, rather than being
maintained by two parallel implementations.
