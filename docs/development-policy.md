# Development Policy

Last updated: 2026-08-01

This document is the standing development policy for `usd-geo-plugins`. The
roadmap, format support order, and architecture documents refine it; they do
not override it.

## 1. Purpose

`usd-geo-plugins` provides OpenUSD FileFormat Plugins that bring geospatial,
survey, and point-cloud data into OpenUSD workflows.

LAS and LAZ are the initial targets. The intended coverage is wider: COPC, PLY,
delimited text point formats (XYZ, PTS, CSV), E57, GeoTIFF and DEM elevation,
COG, and vector formats. The shared contracts must therefore not depend on a
single format or a single OpenUSD version, so that those formats, plus tiling,
LOD, caching, and streaming, can be added later without rewriting the readers.
The order and entry gates are in
[format support order](roadmap/format-support-order.md).

The repository is not a one-shot conversion tool. The goal is a foundation
where an OpenUSD application can reference, inspect, and progressively load
point-cloud data directly.

## 2. Current Assessment

v0.1.0 is a complete first public release. The properties to preserve are:

- Format parsing is separated from OpenUSD API usage.
- LAS and LAZ share one point-cloud authoring path.
- The core libraries and readers build and test without OpenUSD.
- Geospatial, point-cloud, USD authoring, and plugin responsibilities have
  clear boundaries.
- License boundaries for test data, third-party code, and project code are
  recorded.
- Implementation status and roadmap are documented.

The next work tightens the match between documented and implemented behavior,
then strengthens large-data handling, point-format coverage, distribution
licensing, and the diagnostics API.

## 3. Design Principles

### 3.1 Dependency Direction

```text
usdGeoCore
  |-- usdPointCloudCore -- usdLas -- usdLaz
  `-- usdGeoUsd
             |
   geo-las / geo-laz plugin bundles
```

`usdLas` and `usdPointCloudCore` both depend on `usdGeoCore`. `usdLaz` delegates
LAS record interpretation to `usdLas` after decompression.

Rules:

- Core libraries do not depend on the OpenUSD API.
- LAS and LAZ readers never author USD prims.
- OpenUSD-specific representation stays inside `usdGeoUsd`.
- FileFormat Plugins remain thin adapters.
- New OpenUSD features are not embedded directly into format-independent
  contracts.

### 3.2 Format-Independent Contracts

The following belong in the shared layer, not in a format-specific layer:

- CRS and coordinate reference metadata
- Units
- Up axis
- Source origin and local origin
- Spatial bounds
- Point count
- Point attribute schema
- Tile ID and tile hierarchy
- Geometric error
- Chunk information
- Cache key
- Diagnostic codes

### 3.3 Data Preservation

Input meaning is not lost.

- Retain every attribute that can be read.
- Convert to standard USD attributes when a natural mapping exists.
- Keep attributes without a standard representation as primvars or namespaced
  metadata.
- Prefer designs that never discard unknown attributes or Extra Bytes.
- Make lossy conversions explicit.
- Keep the relationship between source and local coordinates recoverable.

## 4. Format Coverage

Sections 4.1 through 4.6 govern LAS and LAZ. Section 4.7 states what carries
over to the formats that follow them.

### 4.1 Versions

LAS 1.2, 1.3, and 1.4 are the primary targets.

LAS 1.0 and 1.1 are candidates only after demand, test data, and implementation
cost are confirmed.

### 4.2 Point Format Coverage

The eventual target is every standard point data record format defined by
LAS 1.4. Current status is tracked in
[supported formats](supported-formats.md).

Primary targets: formats 0, 1, 2, 3, 6, 7, and 8.

Extended targets: formats 4, 5, 9, and 10.

Formats 4, 5, 9, and 10 carry waveform packet information. Accepting the record
length is not sufficient; the following are designed as shared contracts:

- Waveform packet descriptor index
- Waveform data offset
- Waveform packet size
- Return point waveform location
- Waveform parameters
- References to an external waveform data packet file
- A deferred representation for waveform data that has not been fetched

### 4.3 Point Attributes

Supported incrementally, at minimum:

XYZ position, intensity, return number, number of returns, classification,
classification flags, scanner channel, scan direction flag, edge of flight
line, user data, scan angle and scan angle rank, point source ID, GPS time,
RGB, NIR, waveform packet metadata, and Extra Bytes attributes.

Attributes are normalized into one common structure that absorbs per-format
differences. Raw bit layout and scale information are retained where callers
may need to reconstruct the source representation.

### 4.4 CRS

Resolution order:

1. WKT CRS VLR / EVLR
2. GeoTIFF `KeyDirectoryTag`
3. `GeoDoubleParamsTag`
4. `GeoAsciiParamsTag`
5. EPSG code inference
6. Diagnostics for incomplete or conflicting CRS definitions

GeoTIFF information is not ignored when WKT is present. Conflicts produce a
stable diagnostic instead of a silent choice.

### 4.5 VLR and EVLR

Unknown records are not merely skipped; the API can retain them.

Primary targets: `LASF_Projection`, Extra Bytes, waveform packet descriptors,
classification lookup, spatial reference metadata, and application-specific
records.

Both a typed view for known records and a raw view for unknown records are
provided.

### 4.6 Extra Bytes

Extra Bytes support is high priority. Requirements:

- Any number of additional attributes
- Scalar and vector distinction
- Signed, unsigned, and floating-point types
- Scale and offset application
- No-data, minimum, and maximum values
- Deterministic conversion to USD primvar names
- A defined name-collision rule
- Raw bytes preserved even for unknown types

### 4.7 Formats After LAS and LAZ

The next formats are COPC, PLY, delimited text point files, E57, GeoTIFF and
DEM elevation, and COG. They are added under the same rules, with three
additions:

- **No implied georeferencing.** PLY, XYZ, PTS, CSV, and some E57 files carry
  no CRS or unit. A missing CRS is a diagnostic and an explicit file-format
  argument, never a default.
- **Generic attributes come first.** The attribute model built for Extra Bytes
  is the same model PLY properties and text columns map onto. A format-specific
  attribute path is a design failure.
- **Multiple scans are a contract, not a special case.** E57 introduces several
  scans with independent poses in one file. Per-scan pose, bounds, and
  attribute availability belong in the shared point-cloud contracts so that
  LAS remains the single-scan case of the same model.

Terrain rasters use the shared CRS, unit, and local-origin contracts through
`usdTerrainCore`, so elevation and point clouds can be placed on one stage
without a second coordinate model.

## 5. Tile and LOD Policy

### 5.1 Shared Tile Contract

The format-independent tile contract is defined before binding to a concrete
OpenUSD 26.08 API. A tile carries at least:

```text
TileId
ParentTileId
ChildTileIds
SpatialBounds
PointCount
GeometricError
RefinementPolicy
LocalOrigin
SourceRange
SourceChunkIds
AttributeAvailability
```

### 5.2 OpenUSD Representation

`usdGeoUsd` owns the mapping to OpenUSD LOD, payload, variant, activation,
subLayer, and asset dependency. An OpenUSD API change or a different
representation strategy must not require changes to the LAS or LAZ readers.

### 5.3 Initial Sequence

1. Spatial partitioning and tile hierarchy generation
2. Per-tile point count and bounds
3. Deterministic downsampling
4. Geometric error definition
5. Per-tile USD asset generation
6. Integration with the OpenUSD LOD representation
7. API shape for asynchronous and deferred loading

## 6. Streaming and Memory

Designs that materialize a full point cloud in memory are avoided.

### 6.1 Reader API

The reader supports:

- Header-only inspection
- Metadata-only inspection
- Record range decoding
- Chunk iteration
- Attribute selection
- Bounds-based filtering
- Classification filtering
- Return filtering
- Cancellation
- Memory budget

### 6.2 LAZ

LAZ uses the chunk table to decompress only the required range where possible.

Later candidates: spatial-query-to-chunk mapping, chunk cache, decoder instance
pool, sequential prefetch, and parallel decode.

### 6.3 COPC

COPC is the strongest candidate to follow LAS and LAZ.

COPC-specific handling is not mixed into `usd-laz`. Either a separate `usd-copc`
library or an isolated COPC reader module inside `usd-laz` is used. COPC
hierarchy, octree keys, byte ranges, and resolution map onto the shared tile
contract.

## 7. USDC Cache

A USDC cache is a derived cache of the USD representation generated from an
input file, never the source of truth. It must be deletable and reproducible.

The cache key includes at least:

- Source canonical path or content identity
- Source size
- Source modification time
- Source content hash or partial hash
- Plugin version
- Parser version
- OpenUSD compatibility version
- Coordinate transform settings
- Attribute selection
- Tile and LOD settings
- Downsampling settings

## 8. Diagnostics API

String-only error returns are replaced by typed diagnostics. See
[diagnostics contract](architecture/diagnostics.md).

```cpp
enum class DiagnosticCode {
    InvalidSignature,
    UnsupportedVersion,
    UnsupportedPointFormat,
    TruncatedHeader,
    InvalidOffset,
    InvalidRecordLength,
    TruncatedRecord,
    InvalidCrs,
    ConflictingCrs,
    UnsupportedExtraBytesType,
    MissingWaveformData,
    NonFiniteCoordinate,
    DecodeFailure
};

struct Diagnostic {
    DiagnosticCode code;
    Severity severity;
    std::string message;
    std::optional<std::uint64_t> byteOffset;
    std::optional<std::uint64_t> pointIndex;
};
```

Requirements:

- Codes are stable.
- Messages are for humans.
- Byte offset is attached where available.
- Point index is attached where available.
- Warnings are distinguished from fatal errors.
- The plugin layer converts diagnostics into OpenUSD diagnostics.

## 9. Binary Safety

### 9.1 Endianness

LAS is little-endian. Readers do not implicitly assume host endianness. The
preferred solution is explicit little-endian decoding; a compile-time check
that rejects big-endian hosts is the minimum acceptable alternative.

### 9.2 Overflow

Every offset, count, record length, point count, and VLR length is checked for
integer overflow. These expressions are never evaluated without a prior check:

```text
pointDataOffset + pointCount * pointRecordLength
recordOffset + recordLength
chunkOffset + chunkSize
```

### 9.3 Trust Boundary

LAS and LAZ inputs are untrusted.

- Return diagnostics instead of relying on exceptions or assertions.
- Never perform a large allocation from unvalidated input.
- Never pass a source-declared count straight to `reserve`.
- Limit recursion depth.
- Validate decompressed sizes.

## 10. Testing

### 10.1 Unit Tests

Each layer is tested independently: bounds and coordinate transforms, point
attribute schema, LAS header, per-format decoding, VLR/EVLR, CRS, Extra Bytes,
waveform metadata, LAZ chunk decoding, typed diagnostics, and USD metadata
round-trips.

### 10.2 Conformance Corpus

Every point format gets a minimal fixture. Target matrix:

| LAS version | Point format | Required |
| --- | ---: | --- |
| 1.2 | 0 | Yes |
| 1.2 | 1 | Yes |
| 1.2 | 2 | Yes |
| 1.2 | 3 | Yes |
| 1.3 | 4 | Yes |
| 1.3 | 5 | Yes |
| 1.4 | 6 | Yes |
| 1.4 | 7 | Yes |
| 1.4 | 8 | Yes |
| 1.4 | 9 | Yes |
| 1.4 | 10 | Yes |

Both LAS and LAZ fixtures are provided per format where possible.

### 10.3 Negative Corpus

Signature mismatch, truncated header, invalid header size, invalid point data
offset, record length mismatch, point count overflow, truncated VLR, truncated
EVLR, invalid WKT, conflicting CRS, invalid Extra Bytes descriptor, truncated
LAZ chunk, invalid chunk table, and non-finite coordinates.

### 10.4 Fuzzing

Priority order: LAS header inspector, VLR/EVLR reader, point decoder, Extra
Bytes descriptor parser, and the LAZ chunk adapter boundary. libFuzzer targets
first; OSS-Fuzz integration is a later option.

### 10.5 Large Data

Small fixtures stay in CI. Large corpora run manually or nightly.

Measured: points/sec, peak memory, first prim availability time, cache
generation time, cache hit time, tile count, and output size.

## 11. CI and Release

### 11.1 Matrix

Windows, Linux, and macOS; representative Debug and Release configurations;
core-only builds; OpenUSD-enabled builds; and LAS/LAZ plugin integration.

### 11.2 Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are added on Linux CI.
ThreadSanitizer, clang-tidy, and include-what-you-use are added where
practical.

### 11.3 Release Artifacts

A release contains the plugin bundle, `LICENSE`, `NOTICE`,
`THIRD_PARTY_NOTICES.md`, the laz-perf LGPL license, the supported formats
matrix, OpenUSD compatibility information, installation instructions, and
checksums.

## 12. Licensing and Distribution

Project code stays Apache-2.0.

Binary distribution that includes laz-perf must satisfy LGPL-2.1. The concrete
obligations, the link model, and the artifact layout are fixed in
[distribution](distribution.md).

Questions requiring legal interpretation are reviewed by a qualified reviewer
before a public binary release.

## 13. Documentation

Documented support must match implemented behavior exactly. The README states
the current point format range, links the supported formats and attribute
matrices, and lists known limitations. Plugin discovery, `usdview` and `usdcat`
usage, authored prim and metadata examples, large-data behavior, and the LAZ
LGPL distribution note are covered in the README or the documents it links.

## 14. Repository Shape

```text
libs/
  usd-geo-core/
  usd-pointcloud-core/
  usd-las/
    src/
      LasHeaderReader.cpp
      LasMetadataReader.cpp
      LasPointDecoder.cpp
      LasCrs.cpp
      LasExtraBytes.cpp
      LasWaveform.cpp
  usd-laz/
  usd-geo-usd/
plugins/
  geo-las/
  geo-laz/
docs/
  architecture/
  roadmap/
  releases/
  compatibility/
  distribution.md
  supported-formats.md
```

`Las.cpp` is split into internal modules as features are added. Directories are
created when their first tested capability exists.

Later format libraries (`usd-copc`, `usd-ply`, `usd-ascii-points`, `usd-e57`,
`usd-terrain-core`) and their plugin bundles follow the same shape. The full
target layout is in [library architecture](roadmap/library-architecture.md).

## 15. Workstream Priority

### W1: Public specification alignment

- State the exact point format range in the README
- Add the supported formats matrix
- Add known limitations
- Define the base typed diagnostic types
- Introduce an endian-safe binary reader

### W2: LAS attribute coverage

Classification flags, scanner channel, scan angle, user data, point source ID,
NIR, GeoTIFF CRS, and Extra Bytes.

### W3: All standard point formats

Formats 4, 5, 9, and 10; waveform descriptors; waveform packet metadata; and
external waveform data references.

### W4: Large-data foundation

Chunk iterator, range decode, memory budget, cancellation, attribute
selection, bounds filter, and deterministic sampling.

### W5: Tile and LOD

Shared tile contract, hierarchy builder, geometric error, per-tile USD assets,
and the OpenUSD 26.08 LOD mapping.

### W6: Cache and additional formats

USDC cache, COPC, PLY, delimited text point formats, E57, GeoTIFF and DEM
elevation, COG, and remote byte-range sources.

Workstreams map onto the roadmap phases in
[roadmap](roadmap/README.md). Point-format expansion and tile/LOD work may run
in parallel, but the shared point schema and streaming reader API are
stabilized first because both depend on them.

## 16. Tracked Issues

The following are managed as public issues:

1. Document the exact LAS point-format support matrix
2. Introduce endian-safe binary decoding
3. Replace string-only LAS errors with typed diagnostics
4. Add LAS point format 4 decoding
5. Add LAS point format 5 decoding
6. Add LAS point format 9 decoding
7. Add LAS point format 10 decoding
8. Add waveform packet descriptor parsing
9. Add GeoTIFF CRS VLR parsing
10. Add Extra Bytes VLR parsing and generic attributes
11. Add complete LAS 1.4 classification flags
12. Add NIR point attribute support
13. Add chunked and range-based reader API
14. Define shared tile and LOD contracts
15. Add deterministic point-cloud downsampling
16. Map shared LOD contracts to OpenUSD 26.08
17. Define USDC cache identity and invalidation
18. Add LAS/LAZ fuzzing targets
19. Expand CC0/public point-cloud conformance corpus
20. Document LGPL-compliant binary distribution
21. Define the generic point attribute model shared by Extra Bytes, PLY
    properties, and text columns
22. Add PLY point reading
23. Add delimited text point reading with explicit column mapping
24. Extend point-cloud contracts to multiple scans and per-scan poses
25. Add E57 scan reading
26. Add GeoTIFF and DEM elevation reading
27. Add COG overview and range-based raster access

## 17. Definition of Done

A point format counts as supported only when all of the following hold:

- Required fields of the format decode correctly.
- Fixtures exist for both LAS and LAZ.
- Invalid record lengths are rejected.
- Attributes reach the shared point schema.
- Attributes survive the USD authoring path.
- A round-trip or expected-value test exists.
- The supported formats matrix is updated.
- Diagnostics return stable codes.
- License information and fixture provenance are recorded.

## 18. Immediate Actions

1. Make the README point format statement exact.
2. Add [supported formats](supported-formats.md).
3. Land the binary reader and typed diagnostics first.
4. Implement Extra Bytes and GeoTIFF CRS.
5. Define the waveform contract for formats 4, 5, 9, and 10.
6. Collect per-format LAS and LAZ fixtures.
7. Then move to the shared tile and LOD contract.
