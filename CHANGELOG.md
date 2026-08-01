# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Added

- Standing development policy covering contracts, LAS/LAZ scope, tiling,
  streaming, caching, diagnostics, binary safety, testing, and licensing.
- Supported formats document with the exact point format, attribute, VLR, CRS,
  and authored USD matrices.
- Diagnostics contract describing the migration from string errors to typed
  diagnostics.
- Binary distribution document covering the LGPL-2.1 obligations for `geo-laz`.
- OpenUSD compatibility statement for the pinned runtime and tested platforms.
- Roadmap entries for COPC, PLY, delimited text point formats, E57, GeoTIFF and
  DEM elevation, and COG.
- Tile and LOD contract fixing OpenUSD 26.08 `usdLod` as the only public LOD
  representation, with the target namespace, validation invariants, sampling
  and cache-key rules, payload policy, and the test matrix.
- Plugin adapter contract recording that `geo-las` and `geo-laz` do not yet
  meet the thin-adapter rule, and the migration onto one reader contract and
  one shared authoring entry point.
- File-format argument contract covering syntax, candidate arguments,
  validation and normalization rules, layer identity, and cache-key
  participation.
- ADR-0003 proposing a sequence for dynamic file format support, with the open
  questions that must be answered before it is accepted or rejected.

### Changed

- README states the exact LAS point format range, plugin discovery, usage
  examples, the authored prim shape, and known limitations.
- Roadmap phase table reflects the state shipped in v0.1.0.
- Development policy section 5 replaces the open tile/LOD plan with the
  `usdLod` standing policy: no repository-specific LOD schema, tiling separate
  from LOD, deterministic sampling, renderer-driven selection, and payload
  behavior measured rather than assumed.
- OpenUSD compatibility statement records the planned `usdLod` surface and the
  26.08 floor for LOD authoring.
- Supported formats and the README state explicitly that no LOD is authored
  today, and that the chunked reader API is not reachable through the plugins.
- Development policy records file-format arguments and layer identity as a
  design principle, and makes plugin thinning a prerequisite for tile and LOD
  work.

## [0.1.0] - 2026-08-01

### Added

- Shared geospatial values, transforms, bounds, and point-cloud contracts.
- LAS 1.2-1.4 header, VLR/EVLR, WKT CRS, and point-record support.
- LAZ chunk decoding through the bundled `laz-perf` adapter.
- OpenUSD FileFormat Plugins for LAS and LAZ.
- Machine-readable diagnostics for invalid or unsupported input.
- Apache License 2.0 project licensing and third-party notices.

### Known limitations

- Rendering is provided by the consuming OpenUSD application.
- Tile/LOD streaming and a USDC cache are not included in this release.
- OpenUSD is a required runtime dependency for the USD targets.

[0.1.0]: https://github.com/animu-sphere/usd-geo-plugins/releases/tag/v0.1.0
