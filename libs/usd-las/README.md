# usdLas

## Purpose

`usdLas` is the LAS reader: headers, variable-length records, CRS extraction,
point-record decoding, and the chunked orchestration behind `LasReader`. It
turns a `.las` file into the shared `usdPointCloudCore` contracts and knows
nothing about OpenUSD. It is also the LAS-semantics layer for `usdLaz`, which
decompresses records and then hands them here for interpretation.

CMake package `usdLas`, target `usdlas::core`, C++ namespace `usdlas`.

## Responsibilities

- LAS 1.2, 1.3, and 1.4 header inspection, including the 64-bit point count and
  the legacy count fallback.
- VLR and EVLR parsing, with bounds and offset validation.
- Known metadata records: WKT CRS (`LASF_Projection` 2112), GeoTIFF key
  directory and parameter tags (34735-34737), and Extra Bytes descriptors
  (`LASF_Spec` 4).
- Point data record formats 0 through 10, including RGB, NIR, GPS time,
  classification flags, scanner channel, and the waveform packet fields.
- Scalar Extra Bytes decoding with descriptor scale and offset applied.
- Endian-safe decoding of the little-endian on-disk layout.
- Chunked and range-based reads honouring `PointReadOptions`.
- Metadata-only reads: `LasReader::ReadMetadata` parses the header and records
  without touching point data.
- Attribute fan-out into `PointData` and construction of `PointCloudAsset` and
  the metadata-only `PointChunk` / `GeoReference` / `SpatialBounds` triple.

## Non-responsibilities

- OpenUSD authoring of any kind.
- Spatial tiling policy, tile persistence, or payload generation.
- LOD hierarchy construction. `usdLas` delivers points for a range; deciding
  what a level *is* belongs to `usdPointCloudCore` sampling and the authoring
  library.
- Plugin registration or `SDF_FORMAT_ARGS` parsing.
- LAZ decompression — that is `usdLaz`, which depends on this module rather
  than the other way round.

## Public API

```text
usdlas/Las.h
```

| Group | Entry points |
| --- | --- |
| Value types | `LasHeader`, `LasPoint`, `LasVariableLengthRecord`, `LasGeoTiffKey`, `LasGeoTiffMetadata`, `LasExtraBytesDescriptor`, `LasWaveformPacket` |
| Orchestration | `LasReader::Read`, `LasReader::ReadMetadata`, `LasReader::FailureKind`, `LasReadFailure` |
| Buffer inspection | `InspectHeader`, `InspectMetadata`, `InspectRecords`, `ParseKnownMetadata`, `ExtractWktCrs`, `DecodePoint` |
| Shared-contract construction | `AppendPointData`, `BuildPointCloudAsset`, `BuildPointCloudMetadata` |
| Aliases | `LasReadOptions` = `usdpointcloud::PointReadOptions`, `LasPointChunkConsumer`, `LasPointChunkErrorConsumer` |

Minimal use:

```cpp
#include "usdlas/Las.h"

usdlas::LasReader reader(path);
usdlas::LasHeader header;
usdpointcloud::PointData data;
std::vector<usdgeo::Diagnostic> diagnostics;

const bool ok = reader.Read(
    usdlas::LasReadOptions{},
    [&](const usdlas::LasHeader& h, const std::vector<usdlas::LasPoint>& points) {
        std::string error;
        return usdlas::AppendPointData(h, points, path, data, error);
    },
    header,
    diagnostics);
```

Each `Read` overload exists in both a `std::string& error` form and a
`std::vector<usdgeo::Diagnostic>&` form. New code uses the diagnostic form; the
string overloads remain only until every caller has migrated.

## Dependencies

`usdgeo::core` and `usdpointcloud::core`. OpenUSD is **not** required. There is
no third-party decoder: LAS records are decoded in-repo.

## Data flow

```text
.las bytes
    | InspectHeader / InspectMetadata / InspectRecords / ParseKnownMetadata
    v
LasHeader (+ VLRs, CRS, GeoTIFF keys, Extra Bytes descriptors)
    | LasReader::Read, bounded by PointReadOptions
    v
std::vector<LasPoint> chunks -> consumer callback
    | AppendPointData
    v
usdpointcloud::PointData
    | BuildPointCloudAsset
    v
usdpointcloud::PointCloudAsset
```

`ReadMetadata` stops after the header and record stage, and
`BuildPointCloudMetadata` produces the chunk, reference, and bounds without
any `PointData`.

## Error and diagnostic behavior

Nothing throws across the API boundary. Every fallible entry point returns
`bool` and appends `usdgeo::Diagnostic` records — with a byte offset or point
index where the condition is anchored to one — to a caller-owned vector.
`LasReader::FailureKind()` returns a typed `LasReadFailure` so a caller can map
a failure onto its own stable code without parsing a message. Messages are for
humans and may change between releases.

A rejected chunk or a malformed point stops the read. Partial results are not
delivered: the consumer either sees a complete, validated chunk or the read
fails.

## Threading and ownership

`LasReader` holds a filename and per-read failure state; it is **not**
thread-safe and one instance must not be driven from two threads. Separate
instances over the same file are safe because the reader opens and seeks per
read rather than holding a shared handle.

Buffer ownership is explicit. The `std::vector<LasPoint>&` passed to a chunk
consumer is **borrowed** — it is owned by the reader and is valid only for the
duration of the callback, and is reused for the next chunk. A consumer that
needs to keep points must copy or move them out, which is exactly what
`AppendPointData` does. Everything returned by value (`LasHeader`,
`PointData`, `PointCloudAsset`) is owned by the caller.

`PointReadOptions::isCancelled` is checked before each chunk and is invoked on
the calling thread.

## Coordinate-space assumptions

Positions are decoded to `double` in **source** coordinates by applying the
header scale and offset. `usdLas` does not translate by the local origin and
does not convert the up axis; `BuildPointCloudAsset` records the source up
axis as `Z` on the `GeoReference` and leaves the stage conversion to the
authoring library. Bounds produced here are source-space bounds.

## Build and test

Builds and tests with plain CMake and **no OpenUSD runtime**:

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release -R usdLas_unit --output-on-failure
```

A deterministic conformance fixture lives with the plugin bundle at
`plugins/geospatial-las/tests/fixtures/conformance.las`, alongside a committed
real-data corpus under `plugins/geospatial-las/tests/corpus/`.

## Known limitations

- LAS 1.0 and 1.1 are rejected.
- Extra Bytes: scalar types 1-10 only. Vector types, non-finite values, and
  64-bit integers not exactly representable as `double` are rejected. A
  descriptor name that is not already a valid USD identifier fails the file;
  normalization is not implemented.
- Waveform sample data is not fetched or interpreted. Packet offsets, sizes,
  parameters, the external-data flag, and the sibling `.wdp` reference are
  retained for deferred loading.
- GeoTIFF keys are parsed and retained but not interpreted into a CRS; EPSG is
  not inferred and conflicting CRS records are not detected.
- Decoding assumes a little-endian host.
- A read still delivers into a caller-accumulated `PointData`, so the reader
  bounds its own buffers but not the caller's.
- `Las.cpp` is a single translation unit; the header/metadata/decode/CRS/Extra
  Bytes/waveform split described in the design policy has not happened yet.

## Planned work

- `OpenLasPointStream`, the pull-based `PointStream` factory that lets a caller
  consume chunks without accumulating the cloud.
- Chunk-continuity and attribute-preservation tests against the spool round
  trip.
- Extra Bytes descriptor-name normalization.

See [streaming and tiling](../../docs/roadmap/streaming-and-tiling.md).

## Contracts

- [Workspace contract](../../docs/architecture/WORKSPACE.md)
- [Point reader architecture](../../docs/architecture/POINT_READER.md)
- [Diagnostics contract](../../docs/architecture/DIAGNOSTICS.md)
- [Capability matrix](../../docs/reference/CAPABILITY_MATRIX.md)
