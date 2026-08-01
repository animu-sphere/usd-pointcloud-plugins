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

Neither plugin meets the rule yet.

| Concern | `geo-las` | `geo-laz` |
| --- | --- | --- |
| Source file access | In the plugin (`std::ifstream`, explicit seeks) | In `usdLaz` |
| Header and VLR/EVLR reading | In the plugin | In `usdLaz` |
| Point-data truncation checks | In the plugin | In `usdLaz` |
| Per-record decode loop | In the plugin | In `usdLaz` |
| Uses the chunked reader API | No | Yes |
| Attribute fan-out into `PointCloudLayer::Data` | In the plugin | In the plugin |
| `PointChunk` attribute schema | In the plugin | In the plugin |
| `GeoReference` and bounds construction | In the plugin | In the plugin |
| Stage creation, metrics, authoring, transfer | In the plugin | In the plugin |
| `metadataOnly` | Refused | Refused |

`GeoLasFileFormat::Read` is roughly 350 lines. It opens the file itself, reads
a 375-byte header window, reads the VLR and EVLR byte ranges, performs the
`pointDataOffset + pointCount * pointRecordLength` overflow check, and runs its
own per-record `usdlas::DecodePoint` loop.

`usdlas::LasReader` already provides exactly that orchestration behind
`LasReadOptions`, a chunk consumer, and typed diagnostics. It is currently
called only from `libs/usd-las/tests`. `GeoLazFileFormat::Read` does use
`usdlaz::LazReader`, so the LAZ side is one step further along.

Both plugins then duplicate the same tail: per-format `reserve` and `push_back`
branches into `PointCloudLayer::Data`, `PointChunk` attribute construction,
`GeoReference` construction including the CRS-unavailable fallback string, the
bounds transform, and anonymous-layer creation with stage metrics and
`TransferContent`.

### Consequence

Because neither plugin passes read options, `chunkPointLimit`,
`memoryBudgetBytes`, `range`, and `isCancelled` are unreachable through the
plugin layer. The streaming reader described in
[point reader architecture](point-reader.md) exists and is tested, but the
memory limitation documented in the README still holds, because the plugins do
not use it. Closing that gap is the point of this migration, not a later
optimization.

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

1. Move `geo-las` onto `usdlas::LasReader`, deleting the plugin's file access,
   metadata reading, truncation checks, and decode loop. The LAZ plugin already
   shows the shape.
2. Move the shared tail into `usdgeo::AuthorPointCloudAsset`: attribute
   fan-out, `PointChunk` construction, `GeoReference` and bounds, stage
   metrics, and layer transfer. Both plugins call it.
3. Normalize file-format arguments in the plugin and pass them to the reader as
   read options, making `chunkPointLimit`, `memoryBudgetBytes`, `range`, and
   `isCancelled` reachable.
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
