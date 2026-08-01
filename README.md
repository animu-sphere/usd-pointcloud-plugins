# OpenUSD Geospatial Geometry Plugins

[![ost source ci](https://github.com/animu-sphere/usd-geo-plugins/actions/workflows/ost-source-ci.yml/badge.svg?branch=main)](https://github.com/animu-sphere/usd-geo-plugins/actions/workflows/ost-source-ci.yml)

OpenUSD FileFormat Plugins for LAS and LAZ point clouds, with shared
geospatial metadata and point-cloud contracts for surveying and mapping
workflows.

## What It Does

- Imports LAS 1.2-1.4 headers and point formats 0-3 and 6-8 into OpenUSD
  through the `las` file format. Point formats 4, 5, 9, and 10 are planned.
- Imports compressed LAZ point records with the same logical model through the
  `laz` file format.
- Authors `UsdGeomPoints` with point positions, intensity, returns,
  classification, RGB, GPS time, CRS metadata, local origin, bounds, and
  point-count metadata.
- Keeps format-independent validation and coordinate handling outside the USD
  plugin layer.
- Reports stable, machine-readable diagnostics for invalid or unsupported
  input.

Rendering is provided by the consuming OpenUSD application. These plugins
provide import and USD authoring, not a point-cloud renderer.

## Supported Formats

| Extension | Plugin | Current support |
| --- | --- | --- |
| `.las` | `geo-las` | LAS 1.2-1.4 headers, VLR/EVLR metadata, WKT CRS, point formats 0-3 and 6-8 |
| `.laz` | `geo-laz` | The same logical model, decoded through the bundled `laz-perf` adapter |

Point formats 6-8 require LAS 1.4. Formats 4, 5, 9, and 10 are rejected with a
diagnostic because waveform packets are not implemented.

| Attribute | Status |
| --- | --- |
| XYZ, intensity, return number, number of returns, classification | Authored |
| RGB (formats 2, 3, 7, 8) | Authored |
| GPS time (formats 1, 3, 6, 7, 8) | Authored |
| NIR, scan angle, user data, point source ID, classification flags, scanner channel, scan direction, edge of flight line | Authored |
| Extra Bytes, waveform metadata | Not implemented |

The complete matrix, including VLR, CRS, and authored USD attributes, is in
[supported formats](docs/supported-formats.md).

COPC, PLY, delimited text point files (XYZ, PTS, CSV), E57, GeoTIFF and DEM
elevation, and COG are planned in that order; see
[format support order](docs/roadmap/format-support-order.md).

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

## Using the Plugins

### Plugin discovery

An installed bundle has this layout:

```text
<bundle>/lib/GeoLasFileFormat.dll        # or .so / .dylib
<bundle>/plugin/resources/geo-las/plugInfo.json
<bundle>/openstrata.plugin.yaml
```

Point `PXR_PLUGINPATH_NAME` at the directory that holds `plugInfo.json`. A
trailing slash makes OpenUSD search the subdirectories, which registers both
plugins at once:

```powershell
$env:PXR_PLUGINPATH_NAME = "C:\path\to\geo-las\plugin\resources\"
```

```bash
export PXR_PLUGINPATH_NAME=/path/to/geo-las/plugin/resources/
```

The plugin library must be able to load the OpenUSD libraries it was built
against, so keep the runtime used for the build on `PATH` or
`LD_LIBRARY_PATH`.

### Opening a file

```bash
usdview sample.las
usdcat sample.laz
usdcat --flatten sample.las -o sample.usda
```

Any OpenUSD application that discovers FileFormat Plugins can reference a LAS
or LAZ path directly:

```usda
def "Survey" (
    references = @./sample.las@
)
{
}
```

### Authored result

```usda
#usda 1.0
(
    metersPerUnit = 1
    upAxis = "Y"
)

def Points "PointCloud"
{
    point3f[] points = [(0, 0, 0), (1.42, 0.31, -2.07), ...]
    custom uchar[] geo:classification = [2, 2, 6, ...]
    custom int[] geo:intensity = [1024, 981, ...]
    custom uchar[] geo:numberOfReturns = [1, 2, 2, ...]
    custom uchar[] geo:returnNumber = [1, 1, 2, ...]
    custom double3 geo:boundsMin = (0, 0, -84.31)
    custom double3 geo:boundsMax = (512.5, 43.2, 0)
    custom double3 geo:localOrigin = (-8242.5, -34212.5, 12.4)
    custom string geo:linearUnit = "metre"
    custom string geo:upAxis = "Y"
    custom int geo:epsgCode = 0
    custom string geo:wkt = "PROJCS[...]"
    custom uint64 geo:pointCount = 1048576
}
```

Positions are stage-local `float` values. Source coordinates are recovered
from `geo:localOrigin`; the exact expression is in
[supported formats](docs/supported-formats.md).

## Known Limitations

- The whole point cloud is loaded into memory before the layer is authored.
  LAZ decodes in 65,536-point chunks but still accumulates every point. Expect
  memory use proportional to the point count, and plan accordingly for files
  above a few tens of millions of points.
- There are no file-format arguments yet, so attribute selection, point
  limits, and bounds or classification filters are unavailable.
- `metadataOnly` reads are refused; there is no header-only inspection path
  through the plugins.
- CRS comes from the WKT VLR only. GeoTIFF keys are retained but not parsed,
  and EPSG codes are not inferred.
- Point decoding assumes a little-endian host.
- Tile/LOD streaming and a USDC cache are not implemented.
- Writing LAS or LAZ is out of scope; both plugins export as `usda`.

See the [implementation status](docs/roadmap/implementation-status.md) and
[roadmap](docs/roadmap/README.md) for the planned work.

## Status

v0.1.0 is the first public release. It establishes the shared geospatial and
point-cloud contracts, LAS and LAZ readers, and the OpenUSD FileFormat Plugin
integration. See the [release record](docs/releases/v0.1.0.md).

Direction for the work after v0.1.0 is fixed in the
[development policy](docs/development-policy.md).

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
docs/architecture/        Cross-cutting design contracts
docs/compatibility/       Runtime and OpenUSD compatibility statements
docs/roadmap/             Architecture decisions and implementation phases
docs/releases/            Immutable release records
```

## Documentation

- [Development policy](docs/development-policy.md)
- [Supported formats](docs/supported-formats.md)
- [Binary distribution and licensing](docs/distribution.md)
- [OpenUSD compatibility](docs/compatibility/openusd.md)
- [Diagnostics contract](docs/architecture/diagnostics.md)
- [Implementation status](docs/roadmap/implementation-status.md)
- [Roadmap](docs/roadmap/README.md)
- [Release records](docs/releases/README.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

## License

Project code is licensed under the Apache License 2.0; see [LICENSE](LICENSE)
and [NOTICE](NOTICE). The LAZ adapter incorporates `laz-perf 2.0.0`, which is
distributed under LGPL-2.1, and the `geo-laz` plugin binary therefore contains
LGPL-2.1 code. Redistributing `geo-laz` binaries carries the obligations
described in [binary distribution](docs/distribution.md); see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the applicable
third-party terms. The `geo-las` plugin contains no laz-perf code.
