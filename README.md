# OpenUSD Point Cloud Plugins

[![ost source ci](https://github.com/animu-sphere/usd-pointcloud-plugins/actions/workflows/ost-source-ci.yml/badge.svg?branch=main)](https://github.com/animu-sphere/usd-pointcloud-plugins/actions/workflows/ost-source-ci.yml)

OpenUSD FileFormat Plugins and libraries for point-cloud data, including LAS,
LAZ, local COPC reads, spatial tiling, payload-backed authoring, and shared point-cloud contracts
for surveying, mapping, scanning, and 3D data-exchange workflows.

**What it does**

- Imports LAS 1.2-1.4 headers and point formats 0-10 through the `las` file
  format, including waveform packet metadata for formats 4, 5, 9, and 10 and
  scalar Extra Bytes point attributes.
- Imports compressed LAZ point records through the `laz` file format for the
  point formats the bundled `laz-perf` codec supports.
- Authors `UsdGeomPoints` with positions, intensity, returns, classification,
  RGB, NIR, GPS time, waveform metadata, Extra Bytes, CRS metadata, local
  origin, bounds, and point-count metadata.
- Authors OpenUSD 26.08 `usdLod` roots from compact `lod` profiles, and reads
  headers alone through `metadataOnly`.
- Keeps format-independent validation and coordinate handling outside the USD
  plugin layer, so the core libraries test without an OpenUSD runtime.
- Reports stable, machine-readable `LASxxx` / `LAZxxx` diagnostics for invalid
  or unsupported input.

Rendering is the consuming application's responsibility. These plugins provide
import and USD authoring, not a point-cloud renderer, and LOD selection stays
with the host application. See the
[tile and LOD contract](docs/architecture/LOD.md).

## Supported Formats

| Extension | Plugin | Current support |
| --- | --- | --- |
| `.las` | `pointcloud-las` | LAS 1.2-1.4 headers, VLR/EVLR metadata, WKT CRS, GeoTIFF keys, point formats 0-10 |
| `.laz` | `pointcloud-laz` | Point formats 0-3 and 6-8 through the bundled `laz-perf` adapter |
| `.copc` | `pointcloud-copc` | Local COPC metadata and non-tiled reads; source point ranges and tiled reads are currently rejected |

Point formats 4 and 5 require LAS 1.3 or newer; formats 6-10 require LAS 1.4.
The LAZ adapter rejects waveform formats because the bundled `laz-perf` codec
does not provide their compressed record decoders.

| Attribute | Status |
| --- | --- |
| XYZ, intensity, return number, number of returns, classification | Authored |
| RGB (formats 2, 3, 7, 8, 10) | Authored |
| GPS time (formats 1, 3, 6-10) | Authored |
| NIR, scan angle, user data, point source ID, classification flags, scanner channel, scan direction, edge of flight line | Authored |
| Waveform packet metadata and external `.wdp` reference (LAS formats 4, 5, 9, 10) | Authored |
| Extra Bytes point attributes | Types 1-30 authored as scalar/vector USD attributes |

Extra Bytes are authored as `geo:<normalized descriptor name>` with the
descriptor scale and offset applied. Scalar types 1-10 become `double[]`,
vector types 11-20 become `double2[]`, and vector types 21-30 become
`double3[]`. Non-finite values and integer values that are not exactly
representable as `double` are rejected. Names are normalized deterministically
to ASCII USD identifiers; original names remain in header metadata. See the
[capability matrix](docs/reference/CAPABILITY_MATRIX.md).

The complete matrix, including VLR, CRS, and authored USD attributes, is in
the [capability matrix](docs/reference/CAPABILITY_MATRIX.md).

COPC local read support is in progress; PLY, delimited text point files (XYZ, PTS,
CSV), and E57 follow in that order. Terrain, raster, and vector formats are
future repository candidates. See
[format support order](docs/roadmap/format-support-order.md).

## Quick Start

Requirements: CMake 3.23+, a C++17 compiler, OpenUSD 26.08 (the plugin
contract accepts `>=26.08,<27.0`), and OpenStrata 0.21.0 for the pinned
workspace build.

Build and test everything through the pinned OpenStrata workspace:

```powershell
ost configure
ost build
ost test
```

Build and verify a single bundle:

```powershell
ost plugin build .\plugins\pointcloud-las
ost plugin test .\plugins\pointcloud-las --up-to 4
```

Generate a tiled, payload-backed asset explicitly for production processing:

```powershell
usd-pointcloud-convert `
  C:\path\to\sample.las `
  C:\path\to\output\PointCloud.usda `
  --tile-size 128 `
  --memory-limit 1048576
```

The converter is the operational path for long-running tiled generation. It
publishes the root layer and a deterministic `PointCloud.usda.manifest`
sidecar after generation completes; static FileFormat tiled reads remain
available for preview and small inputs. The sidecar records normalized
generation arguments and relative payload asset paths.

Build and test the libraries with plain CMake — without an OpenUSD runtime,
this covers `usdGeoCore`, `usdPointCloudCore`, `usdLas`, and `usdLaz`:

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The full surface is in [BUILDING.md](docs/guides/BUILDING.md).

## Using the Plugins

Point `PXR_PLUGINPATH_NAME` at the directory holding `plugInfo.json`. A
trailing slash makes OpenUSD search subdirectories, registering both bundles at
once:

```powershell
$env:PXR_PLUGINPATH_NAME = "C:\path\to\pointcloud-las\plugin\resources\"
```

```bash
export PXR_PLUGINPATH_NAME=/path/to/pointcloud-las/plugin/resources/
```

Then open a file with any OpenUSD tool, or reference it from a layer:

```bash
usdview sample.las
usdcat --flatten sample.las -o sample.usda
```

```usda
def "Survey" (
    references = @./sample.las:SDF_FORMAT_ARGS:lod=balanced@
)
{
}
```

The supported compact profiles are `off`, `preview`, `balanced`, and
`quality`. `tile=true` connects the LAS/LAZ stream to spill-backed,
payload-backed spatial tiling. `tileSize`, `tileMemoryLimit`, and
`payloadDirectory` control the tiled output. Registration details are in
[INSTALL.md](docs/guides/INSTALL.md); the full argument surface is in the
[file-format argument contract](docs/architecture/FILE_FORMAT_ARGUMENTS.md).

### Previewing without installing

`ost plugin view` prepares the managed OpenUSD runtime and the plugin
environment for a bundle, so a built bundle can be inspected in place:

```powershell
ost plugin view `
  .\plugins\pointcloud-las `
  C:\path\to\sample.las `
  --with .\plugins\pointcloud-laz
```

```bash
ost plugin view \
  ./plugins/pointcloud-las \
  /path/to/sample.las \
  --with ./plugins/pointcloud-laz
```

`--with` makes one session discover both LAS and LAZ. Compact LOD profiles are
passed in the standard USD format-argument suffix:

```powershell
ost plugin view `
  .\plugins\pointcloud-las `
  'C:\path\to\sample.las:SDF_FORMAT_ARGS:lod=balanced' `
  --with .\plugins\pointcloud-laz
```

To run any other USD tool under the same composed environment:

```powershell
ost plugin run .\plugins\pointcloud-las -- usdcat --flatten sample.las -o sample.usda
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
from `geo:localOrigin`; the exact expression is in the
[capability matrix](docs/reference/CAPABILITY_MATRIX.md).

## Tiling and LOD Status

Library capability and FileFormat integration are deliberately reported
separately, because they differ today.

Authoring library (`usdPointCloudAuthoring`):

- `usdLod` hierarchy authoring: implemented
- spatial tiled authoring: implemented
- payload-backed tile output: implemented

LAS/LAZ FileFormat read path:

- compact non-spatial LOD profiles (`lod=preview|balanced|quality`):
  implemented
- spatial `tile` argument: implemented
- bounded-memory payload generation during file open: implemented

The read path consumes bounded pull-stream chunks, spools points by
source-coordinate tile, and authors one payload-backed level per tile. A
Windows baseline against the supplied 14.6-million-point Shizuoka LAS input
is recorded in [streaming and tiling](docs/roadmap/streaming-and-tiling.md);
payload working-set measurements are included, while broader real-world
dataset coverage remains open.

## Known Limitations

- Tiled reads use bounded chunk delivery and spill points to per-tile temporary
  files. A single Shizuoka LAS RSS/spool/payload baseline is published in the
  [streaming and tiling roadmap](docs/roadmap/streaming-and-tiling.md);
  broader real-world dataset coverage remains open.
- File-format arguments expose normalized attribute selection, chunked and
  range-based reads, compact `lod` profiles, spatial tiling, bounds filters,
  and classification filters. The two filters are evaluated in source
  coordinates before stage-local transforms.
- `metadataOnly` reads author the `/PointCloud` metadata namespace without
  decoding point records: source count, bounds, CRS, and available-attribute
  metadata, but no point positions.
- Extra Bytes support covers types 1-30. Non-finite values and integers not
  exactly representable as `double` are rejected; descriptor names are
  normalized deterministically before USD authoring.
- CRS WKT is retained and EPSG is inferred from explicit WKT or GeoTIFF
  horizontal CRS keys. Conflicting definitions are rejected with a typed
  `ConflictingCrs` diagnostic.
- A deterministic USDC cache is not implemented. Payload working-set
  measurements are available for the checked-in real-data processing paths;
  broader dataset coverage remains open.
- Writing LAS or LAZ is out of scope; both plugins export as `usda`.

See the [implementation status](docs/roadmap/implementation-status.md) and
[roadmap](docs/roadmap/README.md) for the planned work.

## Status

Latest release: **v0.2.2** — corrected package and product version metadata;
runtime behavior is unchanged from v0.2.1. The v0.2.0
module and bundle rename is recorded in
[MIGRATION.md](docs/compatibility/MIGRATION.md). See the
[release record](docs/releases/v0.2.2.md) and [CHANGELOG.md](CHANGELOG.md).

Direction is fixed in the [design policy](docs/design/DESIGN_POLICY.md); the
structure is fixed in the
[workspace contract](docs/architecture/WORKSPACE.md).

## Architecture

```text
.las -> usdLas -----------\
                           >-- usdPointCloudAuthoring -- pointcloud-las/laz -> stage
.laz -> usdLaz -> usdLas -/
          ^
          usdGeoCore + usdPointCloudCore
          (shared geospatial and point-cloud contracts)
```

The repository separates format readers, shared validation, USD authoring, and
plugin adapters so the core tests remain independent of an OpenUSD runtime.
The `usdPointCloudTiling` module provides format-independent tile routing and
spool contracts for the streaming authoring path. Readers remain independent
of OpenUSD and the tiling library remains independent of LAS and LAZ; the
plugin adapters connect those pieces through shared APIs. The binding version
of this is
[WORKSPACE.md](docs/architecture/WORKSPACE.md).

## Repository Layout

```text
libs/usd-geo-core/              Geospatial values, transforms, bounds, cache keys, diagnostics
libs/usd-pointcloud-core/       Point attribute, chunk, read-option, sampling, and LOD contracts
libs/usd-pointcloud-authoring/  OpenUSD point-cloud, usdLod, and payload authoring
libs/usd-las/                   LAS header, metadata, and point-record reader
libs/usd-laz/                   LAZ chunk reader and laz-perf adapter
plugins/pointcloud-las/         LAS OpenUSD FileFormat Plugin
plugins/pointcloud-laz/         LAZ OpenUSD FileFormat Plugin
docs/                           See docs/README.md for the documentation index
```

Every module under `libs/` and `plugins/` carries its own `README.md` stating
what it owns and refuses to own; that README is part of the module contract.
See the [module README contract](docs/contributing/MODULE_README_CONTRACT.md).

## Documentation

Start at the [documentation index](docs/README.md).

- [Workspace contract](docs/architecture/WORKSPACE.md) — structure and
  dependency directions
- [Capability matrix](docs/reference/CAPABILITY_MATRIX.md) — what is supported
  today
- [Building and testing](docs/guides/BUILDING.md) ·
  [Installing](docs/guides/INSTALL.md) ·
  [Binary distribution and licensing](docs/guides/DISTRIBUTION.md)
- [Design policy](docs/design/DESIGN_POLICY.md)
- [Tile and LOD contract](docs/architecture/LOD.md) ·
  [Plugin adapter](docs/architecture/PLUGIN_ADAPTER.md) ·
  [File-format arguments](docs/architecture/FILE_FORMAT_ARGUMENTS.md) ·
  [Point reader](docs/architecture/POINT_READER.md) ·
  [Diagnostics](docs/architecture/DIAGNOSTICS.md)
- [Roadmap](docs/roadmap/README.md) ·
  [Streaming and tiling](docs/roadmap/streaming-and-tiling.md) ·
  [Implementation status](docs/roadmap/implementation-status.md)
- [OpenUSD compatibility](docs/compatibility/OPENUSD.md) ·
  [Migration](docs/compatibility/MIGRATION.md)
- [Release records](docs/releases/README.md) ·
  [Third-party notices](THIRD_PARTY_NOTICES.md)

## License

Project code is licensed under the Apache License 2.0; see [LICENSE](LICENSE)
and [NOTICE](NOTICE). The LAZ adapter incorporates `laz-perf 2.0.0`, which is
distributed under LGPL-2.1, and the `pointcloud-laz` plugin binary therefore
contains LGPL-2.1 code. Redistributing `pointcloud-laz` binaries carries the
obligations described in
[binary distribution](docs/guides/DISTRIBUTION.md); see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the applicable
third-party terms. The `pointcloud-las` plugin contains no laz-perf code.
