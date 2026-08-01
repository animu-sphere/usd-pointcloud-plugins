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

### Changed

- README states the exact LAS point format range, plugin discovery, usage
  examples, the authored prim shape, and known limitations.
- Roadmap phase table reflects the state shipped in v0.1.0.

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
