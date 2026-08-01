# Changelog

All notable changes to this project are documented here.

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
