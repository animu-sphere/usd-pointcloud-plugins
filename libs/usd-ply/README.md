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
consumer; bounded streaming and source CRS arguments remain follow-up work.

## Scope

The inspect stage supports ASCII, binary little-endian, and binary big-endian
PLY 1.0 headers. The decode stage accepts scalar vertex properties and rejects
vertex list properties, while preserving non-vertex elements in header metadata.
CRS arguments and OpenUSD authoring are subsequent stages.