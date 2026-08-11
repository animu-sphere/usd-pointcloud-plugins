# Point-Cloud Metadata Contract

This document defines the plain USD attributes authored by the shared
point-cloud authoring path. It is the public data-contract baseline for the
v0.x release line. The implementation remains authoritative when this document
and the current tree disagree.

All attributes in this document are **provisional during v0.x**. Changing a
name, USD type, unit, coordinate space, or semantic meaning requires an
explicit compatibility note. A custom USD schema is intentionally deferred
until these attributes are stable across the supported formats.

## Stage and Prim Shape

The default authored prim is `UsdGeomPoints` at `/PointCloud`. The stage is
Y-up with `metersPerUnit = 1.0`. Tiled and LOD output changes the hierarchy and
payload layout but preserves the metadata semantics below.

`points` is `point3f[]` in stage-local space. Source positions are retained as
double precision while decoding, translated by `geo:localOrigin`, transformed
from source Z-up to stage Y-up, and then authored as floats. Source coordinates
are reconstructed as:

```text
source = (local.x + origin.x,
          -local.z + origin.y,
          local.y + origin.z)
```

`UsdGeomPoints.widths` is authored as a constant display width. When RGB data
is available, `primvars:displayColor` is authored with vertex interpolation.
Those are standard USD properties and are not part of the `geo:*` namespace.

## Dataset Metadata

These attributes are authored on normal and metadata-only point-cloud prims.

| Name | USD type | Meaning | Unit / coordinate space | Required | Source mapping |
| --- | --- | --- | --- | --- | --- |
| `geo:epsgCode` | `int` | EPSG code; `0` when unknown | n/a | Yes | LAS/COPC WKT or GeoTIFF keys; explicit PLY argument |
| `geo:wkt` | `string` | CRS WKT or an unavailability note | n/a | Yes | LAS/COPC WKT; explicit format argument where supported |
| `geo:projJson` | `string` | Reserved PROJJSON representation; currently empty | n/a | Yes | Reserved |
| `geo:linearUnit` | `string` | Source linear-unit name | Source coordinate unit | Yes | Source metadata or explicit argument; `metre` by default |
| `geo:upAxis` | `string` | Stage up axis used by authoring | Stage space | Yes | Shared authoring policy; currently `Y` |
| `geo:localOrigin` | `double3` | Source-space origin subtracted before float authoring | Source coordinates | Yes | Derived from source bounds |
| `geo:boundsMin` | `double3` | Authored point bounds minimum | Stage-local coordinates | Yes | Derived after filtering and coordinate conversion |
| `geo:boundsMax` | `double3` | Authored point bounds maximum | Stage-local coordinates | Yes | Derived after filtering and coordinate conversion |
| `geo:pointCount` | `uint64` | Number of represented source points | Count | Yes | Reader chunk or source metadata |

An unknown EPSG code is represented by `0`; it is not permission to infer a
CRS. Formats without reliable georeferencing require explicit arguments or
report that the CRS is unavailable.

## Point Attributes

Point attributes are optional and, when present, contain one value per authored
point unless noted otherwise.

| Name | USD type | Meaning | Unit / coordinate space | Source mapping |
| --- | --- | --- | --- | --- |
| `geo:intensity` | `int[]` | Sensor return intensity | Source-defined | LAS/LAZ/COPC intensity; PLY `intensity` |
| `geo:returnNumber` | `uchar[]` | Return ordinal | Count | LAS/LAZ/COPC |
| `geo:numberOfReturns` | `uchar[]` | Returns in the pulse | Count | LAS/LAZ/COPC |
| `geo:classification` | `uchar[]` | Point classification | Classification code | LAS/LAZ/COPC; PLY `classification` |
| `geo:classificationFlags` | `uchar[]` | LAS 1.4 classification flags | Bit field | LAS/LAZ/COPC |
| `geo:scannerChannel` | `uchar[]` | Scanner channel | Source-defined | LAS/LAZ/COPC |
| `geo:scanDirectionFlag` | `uchar[]` | Scan direction flag | Boolean encoded as integer | LAS/LAZ/COPC |
| `geo:edgeOfFlightLine` | `uchar[]` | End-of-scan-line flag | Boolean encoded as integer | LAS/LAZ/COPC |
| `geo:userData` | `uchar[]` | Source user data | Source-defined | LAS/LAZ/COPC |
| `geo:scanAngle` | `int[]` | Scan angle or rank | Source format convention | LAS/LAZ/COPC |
| `geo:pointSourceId` | `int[]` | Point source identifier | Source-defined | LAS/LAZ/COPC |
| `geo:red` | `int[]` | Raw red channel | Source integer range | LAS/LAZ/COPC or PLY red aliases |
| `geo:green` | `int[]` | Raw green channel | Source integer range | LAS/LAZ/COPC or PLY green aliases |
| `geo:blue` | `int[]` | Raw blue channel | Source integer range | LAS/LAZ/COPC or PLY blue aliases |
| `geo:nir` | `int[]` | Near-infrared channel | Source integer range | LAS/LAZ/COPC |
| `geo:gpsTime` | `double[]` | GPS time value | LAS GPS-time convention | LAS/LAZ/COPC |
| `geo:waveformDescriptorIndex` | `uchar[]` | Waveform packet descriptor index | Index | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformDataOffset` | `uint64[]` | Waveform packet byte offset | Bytes | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformPacketSize` | `uint[]` | Waveform packet size | Bytes | LAS/COPC formats 4, 5, 9, 10 |
| `geo:returnPointWaveformLocation` | `float[]` | Return location within waveform | Source convention | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformXt` | `float[]` | Waveform X direction parameter | Source coordinates | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformYt` | `float[]` | Waveform Y direction parameter | Source coordinates | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformZt` | `float[]` | Waveform Z direction parameter | Source coordinates | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformDataExternal` | `uchar[]` | Whether waveform bytes are external | Boolean encoded as integer | LAS/COPC formats 4, 5, 9, 10 |
| `geo:waveformDataFile` | `string` | External waveform asset path | Asset identifier | LAS/COPC sibling `.wdp` reference |

Waveform sample bytes are not fetched or interpreted. The attributes preserve
the information needed for deferred loading.

## Generic Attributes

LAS Extra Bytes and generic PLY scalar properties use
`geo:<normalized-source-name>`. Supported USD types are `double[]`,
`double2[]`, and `double3[]`. Values are source-defined and have descriptor
scale and offset applied when the source provides them.

Names are normalized deterministically to ASCII USD identifiers:

- non-alphanumeric characters become `_`;
- a leading digit gains a leading `_`;
- an empty name becomes `extra`;
- collisions with built-in or earlier names gain `_2`, `_3`, and so on.

The normalized name and value type are provisional public API. Original LAS
descriptor names remain available in parsed source metadata.

## Metadata-Only Attributes

Metadata-only reads additionally author:

| Name | USD type | Meaning | Required | Source mapping |
| --- | --- | --- | --- | --- |
| `geo:pointFormat` | `uchar` | Source point-record format | Yes | LAS/LAZ/COPC header |
| `geo:scale` | `double3` | Source integer-coordinate scale | Yes | LAS/LAZ/COPC header |
| `geo:offset` | `double3` | Source integer-coordinate offset | Yes | LAS/LAZ/COPC header |
| `geo:metadataOnly` | `bool` | Marks a prim with no decoded points | Yes | Authoring mode; always `true` |
| `geo:availableAttributes` | `string[]` | Attributes declared by the source | Yes | Reader metadata |

The `points` property exists but is empty for a metadata-only read.

## Compatibility Policy

During v0.x, additions are preferred over semantic changes. A release that
changes an existing name, type, coordinate space, required status, or source
mapping must identify the change in its release record and migration guidance.
Schema design may begin only after this contract has been validated across
LAS, LAZ, COPC, PLY, remote source identity, cache hardening, and adaptive
tiling work.