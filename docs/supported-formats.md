# Supported Formats

This document describes what the readers accept and author **today**. It is
the authoritative support matrix; the README summarizes it. Planned coverage
is in the [development policy](development-policy.md).

Status vocabulary:

- **Supported**: decoded, tested, and authored where an authoring path exists.
- **Read only**: parsed and retained, but not converted into USD attributes.
- **Rejected**: refused with a diagnostic instead of being read partially.

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
record length is accepted, but the trailing bytes are not interpreted.

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
| Extra Bytes | all | Descriptor metadata supported | Raw VLR and typed descriptor metadata retained; point attributes are not decoded yet |

Positions are decoded as `double` in source coordinates using the header scale
and offset, then translated by the local origin and converted to the stage up
axis before being written as `float3`. Extra Bytes descriptors are parsed and
retained, but their appended point-record values are not decoded into the
generic point schema or primvars yet.

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

| Item | Status |
| --- | --- |
| Spatial tiling | Not implemented |
| LOD hierarchy | Not implemented |
| `UsdLodRootAPI` | Not authored |
| `UsdLodScreenSizeHeuristic` | Not authored |
| `UsdLodOverrideAPI` | Not consumed |
| Payload-partitioned tiles | Not authored |
| LOD file-format arguments | Not implemented |

Every read authors one `UsdGeomPoints` prim at `/PointCloud` containing every
point in the source. There is no LOD root, no heuristic, no default index, and
no payload partitioning.

When LOD lands, it will be authored with the OpenUSD 26.08 `usdLod` schemas and
nothing else. The project will not publish a repository-specific LOD schema, a
`lodLevel` primvar, or a variant-set convention, and LOD selection will remain
the host application's responsibility. The target namespace and invariants are
in the [tile and LOD contract](architecture/lod.md).

## Known Limitations

- The whole point cloud is materialized in memory before authoring. LAZ
  decodes in 65,536-point chunks but still accumulates every point.
- File-format arguments currently expose normalized chunk and point-range
  read options plus attribute selection. Bounds, classification filters, and
  LOD arguments remain unavailable. See the
  [file-format argument contract](architecture/file-format-arguments.md) and
  the [plugin adapter contract](architecture/plugin-adapter.md).
- `metadataOnly` reads are refused; a header-only or metadata-only inspection
  path is not exposed through the plugins.
- Positions are stored as `float` relative to the local origin. Absolute source
  precision is preserved only through `geo:localOrigin`.
- Waveform sample data is not fetched or interpreted; packet offsets, sizes,
  parameters, external-data flag, and sibling `.wdp` reference are retained
  for deferred loading.
- Reader errors expose shared typed diagnostics and remain projected to stable
  `LASxxx` or `LAZxxx` plugin prefixes. See
  [diagnostics](architecture/diagnostics.md).
- Tile/LOD streaming and the USDC cache are not implemented. No `usdLod`
  schema is applied to the authored stage.
- Writing LAS or LAZ is out of scope; both plugins export as `usda`.
