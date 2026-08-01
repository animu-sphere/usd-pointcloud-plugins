# Phase 0: Technical Validation

## Objective

Validate the minimum path from an OpenUSD FileFormat Plugin to a LAS / LAZ reader, and establish dependency and performance assumptions.

## Deliverables

- A PoC that opens `.las` and generates `UsdGeomPoints`
- A thin FileFormat Plugin adapter that uses separate reader and USD-authoring libraries
- Precision tests for LAS Scale / Offset and the local origin
- A comparison note for LAS / LAZ reader candidates
- Open time and memory measurements with more than one million points
- Verification of the available OpenUSD version and LOD API

## Exit Criteria

- XYZ and bounds can be expanded into USD without loss
- Source coordinates can be reconstructed from the local origin
- OpenUSD dependencies are confined to `usdGeoUsd` and the plugin adapter
- The LAS reader can be tested without loading an OpenUSD plugin
- The build is reproducible with CMake on Windows

## Open Decisions

- LAZ decoder: use laz-perf behind the `usdLaz` adapter; see [ADR-0002](adr-0002-laz-codec.md)
- OpenUSD API Schema: implement it in `usdGeoUsd` after the Phase 1 data model is settled
- LOD API: verify behavior against the pinned OpenUSD 26.08 runtime

## Validated Environment

- OpenStrata target: `cy2026-windows-x86_64-py313-usd`
- OpenUSD extension: 26.08 with the core feature set
- Build path: `ost configure`, `ost build`, and `ost test`
- Current result: `usdGeoCore` builds and its unit test passes under the pinned runtime
