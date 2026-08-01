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
same LAS validation.

## Point Data Record Formats

| Format | LAS version | Status | Decoded fields |
| ---: | --- | --- | --- |
| 0 | 1.2-1.4 | Supported | XYZ, intensity, returns, classification |
| 1 | 1.2-1.4 | Supported | Format 0 plus GPS time |
| 2 | 1.2-1.4 | Supported | Format 0 plus RGB |
| 3 | 1.2-1.4 | Supported | Format 0 plus GPS time and RGB |
| 4 | 1.3-1.4 | Rejected | Waveform packets are not implemented |
| 5 | 1.3-1.4 | Rejected | Waveform packets are not implemented |
| 6 | 1.4 only | Supported | XYZ, intensity, returns, classification, GPS time |
| 7 | 1.4 only | Supported | Format 6 plus RGB |
| 8 | 1.4 only | Supported | Format 7 fields; NIR is not decoded yet |
| 9 | 1.4 only | Rejected | Waveform packets are not implemented |
| 10 | 1.4 only | Rejected | Waveform packets are not implemented |

Formats 6-10 are accepted only in LAS 1.4. A file that declares format 6, 7,
or 8 in LAS 1.2 or 1.3 is rejected.

A record length shorter than the format minimum (20, 28, 26, 34, 30, 36, and
38 bytes for formats 0, 1, 2, 3, 6, 7, and 8) is rejected. A longer record
length is accepted, but the trailing bytes are not interpreted.

## Point Attributes

| Attribute | Source formats | Status | USD attribute |
| --- | --- | --- | --- |
| XYZ position | all | Supported | `points` (`point3f[]`, stage-local) |
| Intensity | all | Supported | `geo:intensity` (`int[]`) |
| Return number | all | Supported | `geo:returnNumber` (`uchar[]`) |
| Number of returns | all | Supported | `geo:numberOfReturns` (`uchar[]`) |
| Classification | all | Supported | `geo:classification` (`uchar[]`) |
| RGB | 2, 3, 7, 8 | Supported | `geo:red`, `geo:green`, `geo:blue` (`int[]`) |
| GPS time | 1, 3, 6, 7, 8 | Supported | `geo:gpsTime` (`double[]`) |
| Classification flags | 6-8 | Not implemented | - |
| Scanner channel | 6-8 | Not implemented | - |
| Scan direction flag | all | Not implemented | - |
| Edge of flight line | all | Not implemented | - |
| User data | all | Not implemented | - |
| Scan angle / rank | all | Not implemented | - |
| Point source ID | all | Not implemented | - |
| NIR | 8 | Not implemented | - |
| Waveform metadata | 4, 5, 9, 10 | Not implemented | - |
| Extra Bytes | all | Not implemented | Raw VLR data only |

Positions are decoded as `double` in source coordinates using the header scale
and offset, then translated by the local origin and converted to the stage up
axis before being written as `float3`. Extra Bytes appended to a point record
are preserved in neither the point schema nor primvars yet; only the Extra
Bytes VLR itself is retained as raw record data.

## VLR and EVLR

| Record | Status | Notes |
| --- | --- | --- |
| Any VLR | Read only | User ID, record ID, description, raw payload retained |
| Any EVLR (LAS 1.4) | Read only | Same fields, 64-bit length |
| `LASF_Projection` 2112 (WKT) | Supported | Extracted into `geo:wkt` |
| `LASF_Projection` 34735-34737 (GeoTIFF) | Read only | Keys are not parsed |
| `LASF_Spec` 4 (Extra Bytes) | Read only | Descriptors are not parsed |
| Waveform packet descriptors | Read only | Not interpreted |

## CRS

| Source | Status |
| --- | --- |
| WKT VLR / EVLR | Supported |
| GeoTIFF `KeyDirectoryTag` | Not implemented |
| `GeoDoubleParamsTag` / `GeoAsciiParamsTag` | Not implemented |
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

## Known Limitations

- The whole point cloud is materialized in memory before authoring. LAZ
  decodes in 65,536-point chunks but still accumulates every point.
- No file-format arguments exist yet, so attribute selection, point limits,
  bounds filters, and classification filters are unavailable.
- `metadataOnly` reads are refused; a header-only or metadata-only inspection
  path is not exposed through the plugins.
- Positions are stored as `float` relative to the local origin. Absolute source
  precision is preserved only through `geo:localOrigin`.
- Point-record decoding uses `memcpy` on the raw record, so a big-endian host
  is not supported. Endian-safe decoding is planned.
- Errors are string messages carrying a stable `LASxxx` or `LAZxxx` prefix; a
  typed diagnostics API is planned. See
  [diagnostics](architecture/diagnostics.md).
- Tile/LOD streaming and the USDC cache are not implemented.
- Writing LAS or LAZ is out of scope; both plugins export as `usda`.
