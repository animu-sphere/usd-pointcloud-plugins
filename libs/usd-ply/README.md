# usdPly

`usdPly` is the OpenUSD-independent PLY reader foundation. It validates PLY 1.0
headers through vendored tinyply and exposes scalar `vertex` properties through
the shared `PointStream` contract.

CMake package `usdPly`, target `usdply::core`, C++ namespace `usdply`.

## Public API

```text
usdply/Ply.h
```

`InspectHeader` accepts an input stream, returns the PLY encoding, declared
elements, and the first byte after `end_header`. Failures append an anchored
`usdgeo::Diagnostic` and do not return partial metadata.

`OpenPointStream` accepts ASCII and binary PLY sources, including binary
big-endian input, and decodes scalar `x`, `y`, `z`, `intensity`, RGB, and generic
scalar vertex properties into shared point-cloud chunks. The current adapter
uses tinyply's complete payload read and chunks the decoded columns for the
consumer. It applies source bounds, classification filters, cancellation, point
ranges, chunk limits, and the configured memory budget before delivering a
chunk; true bounded source streaming remains follow-up work. `ReadPointCloud`
aggregates the stream into a validated `PointCloudAsset` and applies an explicit
`GeoReference` before authoring.

## Scope

The inspect stage supports ASCII, binary little-endian, and binary big-endian
PLY 1.0 headers. The decode stage accepts scalar vertex properties and rejects
vertex list properties, while preserving non-vertex elements in header metadata.
PLY sources have no embedded CRS. Callers must provide an explicit CRS through
the normalized file-format arguments before authoring.