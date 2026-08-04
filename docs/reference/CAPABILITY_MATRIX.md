# Capability Matrix

This document describes what the readers accept and author **today**. It is
the authoritative support matrix; the README summarizes it. Planned coverage
is in the [design policy](../design/DESIGN_POLICY.md) and the
[roadmap](../roadmap/README.md).

Status vocabulary:

- **Supported**: decoded, tested, and authored where an authoring path exists.
- **Read only**: parsed and retained, but not converted into USD attributes.
- **Rejected**: refused with a diagnostic instead of being read partially.
- **Not connected**: implemented in a library and tested there, but no
  file-format argument reaches it from a direct LAS or LAZ read.

The last distinction matters: the authoring library has supported tiled,
payload-backed output since before any argument could request it. A row that
says "Not connected" is a statement about what opening a `.las` file does, not
about what the repository contains.

## LAS Versions

| Version | Status | Notes |
| --- | --- | --- |
| 1.0 | Rejected | `unsupported LAS version` |
| 1.1 | Rejected | `unsupported LAS version` |
| 1.2 | Supported | 227-byte minimum header, legacy point count |
| 1.3 | Supported | 235-byte minimum header, legacy point count |
| 1.4 | Supported | 375-byte minimum header, 64-bit point count, EVLRs |

LAZ accepts the same versions. The LAZ reader clears the compression bit of the
point data record format byte before header inspection and then applies the
same LAS validation for formats 0-3 and 6-8. The bundled codec does not decode
compressed waveform formats 4, 5, 9, and 10, so those LAZ inputs are rejected.

## Point Data Record Formats

| Format | LAS version | Status | Decoded fields |
| ---: | --- | --- | --- |
| 0 | 1.2-1.4 | Supported | XYZ, intensity, returns, classification |
| 1 | 1.2-1.4 | Supported | Format 0 plus GPS time |
| 2 | 1.2-1.4 | Supported | Format 0 plus RGB |
| 3 | 1.2-1.4 | Supported | Format 0 plus GPS time and RGB |
| 4 | 1.3-1.4 | Supported | Format 1 plus waveform packet metadata |
| 5 | 1.3-1.4 | Supported | Format 3 plus waveform packet metadata |
| 6 | 1.4 only | Supported | XYZ, intensity, returns, classification, GPS time |
| 7 | 1.4 only | Supported | Format 6 plus RGB |
| 8 | 1.4 only | Supported | Format 7 fields plus NIR |
| 9 | 1.4 only | Supported | Format 6 plus waveform packet metadata |
| 10 | 1.4 only | Supported | Format 8 plus waveform packet metadata |

Formats 6-10 are accepted only in LAS 1.4. A file that declares format 6, 7,
or 10 in LAS 1.2 or 1.3 is rejected.

A record length shorter than the format minimum (20, 28, 26, 34, 57, 63, 30,
36, 38, 59, and 67 bytes for formats 0 through 10) is rejected. A longer
record length is accepted when its trailing bytes are declared by an Extra
Bytes VLR.

## Point Attributes

| Attribute | Source formats | Status | USD attribute |
| --- | --- | --- | --- |
| XYZ position | all | Supported | `points` (`point3f[]`, stage-local) |
| Intensity | all | Supported | `geo:intensity` (`int[]`) |
| Return number | all | Supported | `geo:returnNumber` (`uchar[]`) |
| Number of returns | all | Supported | `geo:numberOfReturns` (`uchar[]`) |
| Classification | all | Supported | `geo:classification` (`uchar[]`) |
| RGB | 2, 3, 7, 8, 10 | Supported | `geo:red`, `geo:green`, `geo:blue` (`int[]`) |
| GPS time | 1, 3, 6, 7, 8, 9, 10 | Supported | `geo:gpsTime` (`double[]`) |
| Classification flags | 6-10 | Supported | `geo:classificationFlags` (`uchar[]`) |
| Scanner channel | 6-10 | Supported | `geo:scannerChannel` (`uchar[]`) |
| Scan direction flag | all | Supported | `geo:scanDirectionFlag` (`uchar[]`) |
| Edge of flight line | all | Supported | `geo:edgeOfFlightLine` (`uchar[]`) |
| User data | all | Supported | `geo:userData` (`uchar[]`) |
| Scan angle / rank | all | Supported | `geo:scanAngle` (`int[]`) |
| Point source ID | all | Supported | `geo:pointSourceId` (`int[]`) |
| NIR | 8, 10 | Supported | `geo:nir` (`int[]`) |
| Waveform descriptor index | 4, 5, 9, 10 | Supported | `geo:waveformDescriptorIndex` (`uchar[]`) |
| Waveform data offset | 4, 5, 9, 10 | Supported | `geo:waveformDataOffset` (`uint64[]`) |
| Waveform packet size | 4, 5, 9, 10 | Supported | `geo:waveformPacketSize` (`uint[]`) |
| Return point waveform location | 4, 5, 9, 10 | Supported | `geo:returnPointWaveformLocation` (`float[]`) |
| Waveform parameters | 4, 5, 9, 10 | Supported | `geo:waveformXt`, `geo:waveformYt`, `geo:waveformZt` (`float[]`) |
| External waveform data flag | 4, 5, 9, 10 | Supported | `geo:waveformDataExternal` (`uchar[]`) |
| External waveform data file | 4, 5, 9, 10 | Supported | `geo:waveformDataFile` (`string`) |
| Extra Bytes | all | Types 1-30 supported when integer values are exactly representable as `double` | `geo:<normalized descriptor name>` (`double[]`, `double2[]`, or `double3[]`), with per-component descriptor scale and offset applied |

Positions are decoded as `double` in source coordinates using the header scale
and offset, then translated by the local origin and converted to the stage up
axis before being written as `float3`. Extra Bytes values are decoded from the
appended point-record data and written after applying their per-component
descriptor scale and offset. Types 1-10 are scalar, 11-20 are two-component,
and 21-30 are three-component. Non-finite values and integer values that
cannot be represented exactly as `double` are rejected.
Descriptor names are normalized deterministically to ASCII USD identifiers:
non-alphanumeric characters become `_`, a leading digit gains a leading `_`,
an empty name becomes `extra`, and collisions with built-in or prior names gain
`_2`, `_3`, and so on. The original descriptor names remain in the LAS header
metadata.

## VLR and EVLR

| Record | Status | Notes |
| --- | --- | --- |
| Any VLR | Read only | User ID, record ID, description, raw payload retained |
| Any EVLR (LAS 1.4) | Read only | Same fields, 64-bit length |
| `LASF_Projection` 2112 (WKT) | Supported | Extracted into `geo:wkt` |
| `LASF_Projection` 34735-34737 (GeoTIFF) | Supported | Key directory, double parameters, and ASCII parameters are parsed |
| `LASF_Spec` 4 (Extra Bytes) | Supported | Descriptor type, options, name, limits, scale, offset, and description are parsed |
| Waveform packet descriptors | Read only | Raw descriptors retained; per-point packet metadata is decoded |

## CRS

| Source | Status |
| --- | --- |
| WKT VLR / EVLR | Supported |
| GeoTIFF `KeyDirectoryTag` | Parsed | Structured key directory is retained on the LAS header |
| `GeoDoubleParamsTag` / `GeoAsciiParamsTag` | Parsed | Structured parameter arrays and text are retained on the LAS header |
| EPSG inference | Not implemented |
| Conflicting CRS detection | Not implemented |

When no WKT record is present, the plugins author a placeholder string in
`geo:wkt` stating that the CRS is unavailable, and `geo:epsgCode` is `0`.
`geo:projJson` is currently always empty.

## Authored USD

Both plugins produce the same layer shape.

| Item | Value |
| --- | --- |
| Prim path | `/PointCloud` |
| Prim type | `UsdGeomPoints` |
| Stage up axis | `Y` |
| Stage metersPerUnit | `1.0` |
| Local origin | Source-space minimum of the header bounds |

Dataset metadata authored on the prim:

| Attribute | Type | Meaning |
| --- | --- | --- |
| `geo:epsgCode` | `int` | EPSG code, `0` when unknown |
| `geo:wkt` | `string` | Source WKT, or an unavailability note |
| `geo:projJson` | `string` | Reserved; currently empty |
| `geo:linearUnit` | `string` | Linear unit name, `metre` by default |
| `geo:upAxis` | `string` | Stage up axis used for the conversion |
| `geo:localOrigin` | `double3` | Source-space origin subtracted from points |
| `geo:boundsMin` | `double3` | Stage-local bounds minimum |
| `geo:boundsMax` | `double3` | Stage-local bounds maximum |
| `geo:pointCount` | `uint64` | Point count authored from the header |

Source coordinates are reconstructed as
`sourceUpAxisTransform(localPosition) + geo:localOrigin`, where the source up
axis is `Z` and the stage up axis is `Y`, so
`source = (x + originX, -z + originY, y + originZ)`.

## Tile and LOD

Authoring-library capability and what a direct LAS or LAZ read reaches are
reported separately, because they differ.

| Item | Authoring library | LAS/LAZ FileFormat read |
| --- | --- | --- |
| `usdLod` hierarchy authoring | Supported | Supported via `lod` profiles |
| `UsdLodRootAPI` | Supported | Authored when `lod` is not `off` |
| `UsdLodScreenSizeHeuristic` | Supported | Authored when `lod` is not `off` |
| `UsdLodOverrideAPI` | Not authored | Not authored |
| Deterministic fixed-stride sampling | Supported | Supported |
| Spatial tiling into per-tile `usdLod` roots | Supported | Supported |
| Payload-backed tile assets (one USDC payload per tile/LOD) | Supported | Supported |
| Bounded-memory generation during file open | Implemented for tiled reads; measurement pending | Spool buffers and one-tile reconstruction are bounded; large-corpus measurement not implemented |
| LOD file-format arguments | n/a | `lod=off\|preview\|balanced\|quality` |
| Spatial tile file-format arguments | n/a | `tile=true`, `tileSize`, `tileMemoryLimit`, `payloadDirectory` |

With `lod=off` every read authors one `UsdGeomPoints` prim at `/PointCloud`.
The other profiles author a single non-tiled `usdLod` root with fixed-stride
levels and a screen-size heuristic.

Direct LAS and LAZ FileFormat reads now stream decoded chunks into tile
spools and author one payload tile at a time. Payload working-set behavior is
still not fully measured, and large-corpus benchmark coverage remains planned in
[streaming and tiling](../roadmap/streaming-and-tiling.md).

LOD is authored with the OpenUSD 26.08 `usdLod` schemas and nothing else. The project will not publish a repository-specific LOD schema, a
`lodLevel` primvar, or a variant-set convention, and LOD selection will remain
the host application's responsibility. The target namespace and invariants are
in the [tile and LOD contract](../architecture/LOD.md).

## Known Limitations

- Tiled reads spool points and reconstruct one tile at a time, but large-corpus
  RSS measurement and payload working-set measurement are not yet published.
- Extra Bytes support covers types 1-30. Non-finite values and integers not
  exactly representable as `double` are rejected. Descriptor names are
  normalized before USD authoring.
- File-format arguments currently expose normalized chunk and point-range
  read options, attribute selection, and compact LOD profiles. Spatial tiling,
  bounds, and classification filters remain unavailable. See the
  [file-format argument contract](../architecture/FILE_FORMAT_ARGUMENTS.md) and
  the [plugin adapter contract](../architecture/PLUGIN_ADAPTER.md).
- `metadataOnly` reads author source metadata without decoding point records;
  they do not produce point positions.
- Positions are stored as `float` relative to the local origin. Absolute source
  precision is preserved only through `geo:localOrigin`.
- Waveform sample data is not fetched or interpreted; packet offsets, sizes,
  parameters, external-data flag, and sibling `.wdp` reference are retained
  for deferred loading.
- Reader errors expose shared typed diagnostics and remain projected to stable
  `LASxxx` or `LAZxxx` plugin prefixes. See
  [diagnostics](../architecture/DIAGNOSTICS.md).
- Tile planning is not yet exposed through LAS/LAZ file-format arguments, and
  payload working-set measurement plus the USDC cache remain open. The shared
  USD authoring library supports tiled and payload-backed LOD roots.
- Point decoding assumes a little-endian host.
- Writing LAS or LAZ is out of scope; both plugins export as `usda`.
