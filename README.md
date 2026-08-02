# OpenUSD Geospatial Geometry Plugins

[![ost source ci](https://github.com/animu-sphere/usd-geo-plugins/actions/workflows/ost-source-ci.yml/badge.svg?branch=main)](https://github.com/animu-sphere/usd-geo-plugins/actions/workflows/ost-source-ci.yml)

OpenUSD FileFormat Plugins for LAS and LAZ point clouds, with shared
geospatial metadata and point-cloud contracts for surveying and mapping
workflows.

## What It Does

- Imports LAS 1.2-1.4 headers and point formats 0-10 into OpenUSD through the
  `las` file format, including waveform packet metadata for formats 4, 5, 9,
  and 10.
- Imports compressed LAZ point records through the `laz` file format for the
  point formats supported by the bundled codec.
- Authors `UsdGeomPoints` with point positions, intensity, returns,
  classification, RGB, GPS time, CRS metadata, local origin, bounds, and
  point-count metadata.
- Keeps format-independent validation and coordinate handling outside the USD
  plugin layer.
- Reports stable, machine-readable diagnostics for invalid or unsupported
  input.

Rendering is provided by the consuming OpenUSD application. These plugins
provide import and USD authoring, not a point-cloud renderer. Level of detail
follows the same split: when tiling and LOD land, the plugins will author the
OpenUSD 26.08 `usdLod` hierarchy and its heuristics, and the host application
will select the active LOD. See the
[tile and LOD contract](docs/architecture/lod.md).

## Supported Formats

| Extension | Plugin | Current support |
| --- | --- | --- |
| `.las` | `geo-las` | LAS 1.2-1.4 headers, VLR/EVLR metadata, WKT CRS, point formats 0-10 |
| `.laz` | `geo-laz` | Point formats 0-3 and 6-8 through the bundled `laz-perf` adapter |

Point formats 4 and 5 require LAS 1.3 or newer; formats 6-10 require LAS 1.4.
The LAZ adapter rejects waveform formats because the bundled `laz-perf` codec
does not provide their compressed record decoders.

| Attribute | Status |
| --- | --- |
| XYZ, intensity, return number, number of returns, classification | Authored |
| RGB (formats 2, 3, 7, 8) | Authored |
| GPS time (formats 1, 3, 6, 7, 8) | Authored |
| NIR, scan angle, user data, point source ID, classification flags, scanner channel, scan direction, edge of flight line | Authored |
| Waveform packet metadata and external `.wdp` reference (LAS formats 4, 5, 9, 10) | Authored |
| Extra Bytes point attributes | Not implemented |

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

To build an individual OpenStrata plugin bundle, pass the bundle directory:

```powershell
ost plugin build .\plugins\geo-las
ost plugin build .\plugins\geo-laz
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

On Windows, `ost plugin view` prepares the managed OpenUSD runtime and plugin
environment for a bundle. The LAS preview command is:

```powershell
ost plugin view `
  .\plugins\geo-las `
  C:\path\to\sample.las `
  --with .\plugins\geo-laz
```

The `--with` option is useful when the same session should discover both LAS
and LAZ. Compact LOD preview arguments can be passed in the standard USD
format-argument suffix:

```powershell
ost plugin view `
  .\plugins\geo-las `
  'C:\path\to\sample.las:SDF_FORMAT_ARGS:lod=balanced' `
  --with .\plugins\geo-laz
```

The supported compact profiles are `off`, `preview`, `balanced`, and
`quality`. Spatial `tile=true` preview is not available yet; it is rejected
until spatial tiling is connected to the plugin read path.

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
- File-format arguments currently expose normalized attribute selection,
  chunked and range-based reads, and compact `lod` profiles. Bounds and
  classification filters are unavailable; see the [file-format argument
  contract](docs/architecture/file-format-arguments.md).
- `metadataOnly` reads author the `/PointCloud` metadata namespace without
  decoding point records. The stage contains source count, bounds, CRS, and
  available-attribute metadata, but no point positions.
- CRS comes from the WKT VLR only. GeoTIFF keys are retained but not parsed,
  and EPSG codes are not inferred.
- Point decoding assumes a little-endian host.
- Spatial tile streaming, payload packaging, and a USDC cache are not
  implemented. `lod=preview|balanced|quality` authors a single non-tiled
  `usdLod` root with fixed-stride point samples.
- Writing LAS or LAZ is out of scope; both plugins export as `usda`.

See the [implementation status](docs/roadmap/implementation-status.md) and
[roadmap](docs/roadmap/README.md) for the planned work.

## Status

v0.1.0 is the first public release. It establishes the shared geospatial and
point-cloud contracts, LAS and LAZ readers, and the OpenUSD FileFormat Plugin
integration. See the [release record](docs/releases/v0.1.0.md).

Direction for the work after v0.1.0 is fixed in the
[development policy](docs/development-policy.md). The next major capability is
tiling and level of detail, whose public representation is fixed as OpenUSD
26.08 `usdLod` in the [tile and LOD contract](docs/architecture/lod.md).

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
- [Tile and LOD contract](docs/architecture/lod.md)
- [Plugin adapter contract](docs/architecture/plugin-adapter.md)
- [File-format argument contract](docs/architecture/file-format-arguments.md)
- [Point reader architecture](docs/architecture/point-reader.md)
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
