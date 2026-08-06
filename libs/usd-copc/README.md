# usdCopc

`usdCopc` is the OpenUSD-independent COPC reader foundation. It validates the
LAS 1.4 metadata required by COPC, reads the COPC Info and hierarchy VLRs, walks
local hierarchy pages into format-independent hierarchy entries, and decodes
selected local point-data chunks through the shared LAZ codec.

The module deliberately does not make network requests, write COPC, or depend
on OpenUSD. A streaming point reader and a thin FileFormat Plugin remain open.

## Public API

```text
usdcopc/Copc.h
```

| Type | Purpose |
| --- | --- |
| `CopcInfo` | COPC Info VLR values, including the root hierarchy byte range |
| `CopcHierarchyEntry` | A point-data or child-hierarchy entry from a COPC page |
| `CopcHeader` | Validated `usdlas::LasHeader`, COPC Info, and file size |
| `CopcNode` / `CopcHierarchy` | Validated native hierarchy nodes and spatial bounds |
| `CopcPointTile` | A shared point tile paired with its COPC point-data byte range |
| `CopcReader` | Local metadata, hierarchy-page, and selected point-chunk reader |

`CopcReader::ReadMetadata` delegates LAS header, VLR, EVLR, and CRS parsing to
`usdlas::LasReader::ReadMetadata`. It then requires LAS 1.4 point formats 6
through 8, a first-position 160-byte `copc` Info VLR (record ID 1), and a
`copc` hierarchy VLR (record ID 1000). The reader validates finite Info values,
reserved Info fields, the root hierarchy range, page alignment, child-page
ranges, point-data ranges, and repeated hierarchy pages.

## Current boundary

The hierarchy is returned in deterministic depth-first page order. A positive
`pointCount` identifies a point-data byte range; `-1` identifies a child
hierarchy page; and zero identifies an empty entry. `ReadPointData` reads only
the selected range, and `ReadPoints` decodes that range as a LAZ chunk into
`usdlas::LasPoint` values. `BuildPointTiles` maps point-data nodes to the
shared `PointTile` contract, preserving node IDs, bounds, child relationships,
point counts, native spacing, and the source `pointDataOffset` /
`pointDataSize` in `CopcPointTile`. Hierarchy-page entries are indexing nodes
and are compressed out of the shared tile list.

The initial reader is local and read-only. HTTP range sources, network caching,
COPC writing, hierarchy optimization, and OpenUSD authoring remain outside this
module. The OpenUSD adapter lives in `plugins/pointcloud-copc`.

## Build and test

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release --target usdCopc_tests
ctest --test-dir build -C Release -R usdCopc_unit --output-on-failure
```
