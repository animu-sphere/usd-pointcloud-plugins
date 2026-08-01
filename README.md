# usd-geo-plugins

OpenUSD plugins and format-independent geospatial libraries for surveying,
point-cloud, terrain, and vector-data workflows.

## Status

The repository is in Phase 1 of the roadmap. The current implementation
contains:

- `usdGeoCore`: CRS metadata, explicit source/local coordinate transforms,
  spatial bounds, tile IDs, and stable cache-key inputs.
- `usdPointCloudCore`: OpenUSD-independent point-attribute and point-chunk
  validation contracts.
- `usdGeoUsd`: `UsdGeomPoints` authoring with CRS, local-origin, bounds, and
  point-count metadata.
- `usdLas`: LAS 1.2-1.4 header inspection and uncompressed point-record
  decoding.

The next planned milestone is shared tile / LOD support after LAS and LAZ
FileFormat Plugin integration. See [implementation status](docs/roadmap/implementation-status.md)
and the [roadmap](docs/roadmap/README.md) for the full sequence.

## Requirements

- CMake 3.23 or newer
- C++17 compiler
- OpenStrata 0.20.0 for the pinned USD workspace build
- The `cy2026-windows-x86_64-py313-usd` OpenStrata target for OpenUSD work

The core libraries and their tests do not require an OpenUSD runtime.

## Build And Test

For a regular CMake build on Windows:

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release --target ALL_BUILD
ctest --test-dir build -C Release --output-on-failure
```

For the pinned OpenStrata environment:

```powershell
ost configure
ost build
ost test
```

The generated build directory is intentionally kept separate from source
libraries. Do not add generated OpenStrata or CMake output to source changes.

## Repository Layout

```text
docs/roadmap/             Architecture decisions and implementation phases
libs/usd-geo-core/        Shared geospatial value types and contracts
libs/usd-pointcloud-core/ Format-independent point-cloud contracts
libs/usd-geo-usd/         OpenUSD metadata and point-cloud authoring
```

Format readers, OpenUSD authoring, cache support, and plugin adapters are kept
in separate libraries so that pure data-model tests remain independent of
optional runtimes and codecs.

## Formatting

The repository uses the root [.clang-format](.clang-format) configuration.
Format C++ files with:

```powershell
clang-format -i path/to/file.cpp path/to/file.h
```

Use the formatter version provided by the project toolchain when available.