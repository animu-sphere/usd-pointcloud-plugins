# usdPly

`usdPly` is the OpenUSD-independent PLY reader foundation. Its initial inspect
stage validates a PLY 1.0 header and preserves declared elements and arbitrary
scalar or list properties for a later decode stage.

CMake package `usdPly`, target `usdply::core`, C++ namespace `usdply`.

## Public API

```text
usdply/Ply.h
```

`InspectHeader` accepts an input stream, returns the PLY encoding, declared
elements, and the first byte after `end_header`. Failures append an anchored
`usdgeo::Diagnostic` and do not return partial metadata.

## Scope

The inspect stage supports ASCII, binary little-endian, and binary big-endian
PLY 1.0 headers. It recognizes the standard scalar names and arbitrary vertex
or non-vertex elements. Point decoding, CRS arguments, and OpenUSD authoring
are subsequent stages.