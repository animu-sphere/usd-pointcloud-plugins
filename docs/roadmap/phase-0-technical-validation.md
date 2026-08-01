# Phase 0: Technical Validation

## Objective

Validate the minimum path from an OpenUSD FileFormat Plugin to a LAS / LAZ reader, and establish dependency and performance assumptions.

## Deliverables

- A PoC that opens `.las` and generates `UsdGeomPoints`
- A thin FileFormat Plugin adapter that uses separate reader and USD-authoring libraries
- Precision tests for LAS Scale / Offset and the local origin
- A comparison note for LAS / LAZ reader candidates
- Open time and memory measurements with more than one million points
- Verification of the available OpenUSD version and the `usdLod` schema surface
  (`UsdLodRootAPI`, `UsdLodScreenSizeHeuristic`, `UsdLodOverrideAPI`), including
  the exact authored property names

## Exit Criteria

- XYZ and bounds can be expanded into USD without loss
- Source coordinates can be reconstructed from the local origin
- OpenUSD dependencies are confined to `usdGeoUsd` and the plugin adapter
- The LAS reader can be tested without loading an OpenUSD plugin
- The build is reproducible with CMake on Windows

## Open Decisions

- LAZ decoder: use laz-perf behind the `usdLaz` adapter; see [ADR-0002](adr-0002-laz-codec.md)
- OpenUSD API Schema: no repository-specific LOD schema is defined; the public
  representation is `usdLod`. See the [tile and LOD contract](../architecture/lod.md)
- LOD API: verify the `usdLod` schema surface and behavior against the pinned
  OpenUSD 26.08 runtime

## Validated Environment

- OpenStrata target: `cy2026`, profile `usd`, CLI 0.21.0
- OpenUSD extension: 26.08 with the core feature set; plugin manifests declare
  `>=26.08,<27.0`
- Build path: `ost configure`, `ost build`, and `ost test`
- Current result: the core libraries, `geo-las`, and `geo-laz` build and test
  on Windows x86_64, Linux x86_64, and macOS arm64 under the pinned runtime

See [OpenUSD compatibility](../compatibility/openusd.md) for the full
statement.

## Outstanding

- Open time and peak memory measurements above one million points
- Verification of the OpenUSD 26.08 `usdLod` mechanism against a real tile
  hierarchy, including whether payloads beneath non-selected LOD children are
  composed and whether Storm selection reduces the working set
