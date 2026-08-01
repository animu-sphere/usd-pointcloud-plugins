# OpenUSD Geospatial Geometry Plugins

[![ost source ci](https://github.com/animu-sphere/usd-geo-plugins/actions/workflows/ost-source-ci.yml/badge.svg?branch=main)](https://github.com/animu-sphere/usd-geo-plugins/actions/workflows/ost-source-ci.yml)

OpenUSD FileFormat Plugins for LAS and LAZ point clouds, with shared
geospatial metadata and point-cloud contracts for surveying and mapping
workflows.

## What It Does

- Imports LAS 1.2-1.4 files into OpenUSD through the `las` file format.
- Imports compressed LAZ point records through the `laz` file format.
- Authors `UsdGeomPoints` with point positions, CRS metadata, local origin,
  bounds, and point-count metadata.
- Keeps format-independent validation and coordinate handling outside the USD
  plugin layer.
- Reports stable, machine-readable diagnostics for invalid or unsupported
  input.

Rendering is provided by the consuming OpenUSD application. These plugins
provide import and USD authoring, not a point-cloud renderer.

## Supported Formats

| Extension | Plugin | Current support |
| --- | --- | --- |
| `.las` | `geo-las` | LAS 1.2-1.4 headers, VLR/EVLR metadata, WKT CRS, and point records |
| `.laz` | `geo-laz` | LAZ chunk decoding through the bundled `laz-perf` adapter |

Tile / LOD streaming and a USDC cache are planned follow-up work. See the
[implementation status](docs/roadmap/implementation-status.md) and
[roadmap](docs/roadmap/README.md).

## Quick Start

Requirements:

- CMake 3.23 or newer
- A C++17 compiler
- OpenUSD 26.08, with the plugin contract accepting versions before 27.0
- OpenStrata 0.21.0 for the pinned workspace build

Build and test the libraries with plain CMake:

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release --target ALL_BUILD
ctest --test-dir build -C Release --output-on-failure
```

Build and test the OpenUSD plugins with the pinned OpenStrata workspace:

```powershell
ost configure
ost build
ost test
```

Open a LAS or LAZ path with any OpenUSD application that discovers FileFormat
Plugins, such as `usdview` configured with the corresponding plugin bundle.

## Status

v0.1.0 is the first public release. It establishes the shared geospatial and
point-cloud contracts, LAS and LAZ readers, and the OpenUSD FileFormat Plugin
integration. See the [release record](docs/releases/v0.1.0.md).

## Architecture

```text
.las -> usdLas -> usdGeoUsd -> geo-las -> OpenUSD stage
.laz -> usdLaz -> usdLas  -> usdGeoUsd -> geo-laz -> OpenUSD stage
                 ^
                 shared geo and point-cloud contracts
```

The repository separates format readers, shared validation, USD authoring, and
plugin adapters so the core tests remain independent of an OpenUSD runtime.

## Repository Layout

```text
libs/usd-geo-core/        Geospatial values, transforms, bounds, and cache keys
libs/usd-pointcloud-core/ Point-attribute and chunk validation contracts
libs/usd-geo-usd/         OpenUSD metadata and point authoring
libs/usd-las/             LAS header and point-record reader
libs/usd-laz/             LAZ chunk reader and laz-perf adapter
plugins/geo-las/          LAS OpenUSD FileFormat Plugin
plugins/geo-laz/          LAZ OpenUSD FileFormat Plugin
docs/roadmap/             Architecture decisions and implementation phases
docs/releases/            Immutable release records
```

## Documentation

- [Release records](docs/releases/README.md)
- [Implementation status](docs/roadmap/implementation-status.md)
- [Roadmap](docs/roadmap/README.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## License

Project code is licensed under the Apache License 2.0; see [LICENSE](LICENSE)
and [NOTICE](NOTICE). The LAZ adapter incorporates `laz-perf 2.0.0`, which is
distributed under LGPL-2.1; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
for the applicable third-party terms.