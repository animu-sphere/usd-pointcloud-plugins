# usdLaz

## Purpose

`usdLaz` decompresses LAZ point records through the vendored laz-perf codec and
converts them into the same LAS and point-cloud contracts `usdLas` produces, so
a `.laz` file reaches OpenUSD by exactly the path a `.las` file does. It owns
the only third-party codec in the workspace and is the only module carrying an
LGPL-2.1 obligation.

CMake package `usdLaz`, target `usdlaz::core`, C++ namespace `usdlaz`.

## Responsibilities

- laz-perf integration, fully isolated behind the `LazDecoder` interface.
- Header inspection: the compression bit of the point data record format byte
  is cleared before the shared LAS validation runs.
- Decoding of the compressed point-record formats the bundled codec supports.
- Chunked reads honouring `PointReadOptions`, and range-based reads to the
  extent a sequential codec allows.
- Metadata-only reads through `LazReader::ReadMetadata`.
- Stable diagnostics for formats the codec cannot decode, so an unsupported
  file is refused rather than partially read.

## Non-responsibilities

- LAS record *interpretation*. After decompression, `usdLas` owns header
  semantics, attribute fan-out, CRS extraction, and `PointCloudAsset`
  construction. `usdLaz` never duplicates that logic.
- OpenUSD authoring.
- Spatial tiling policy or payload generation.
- Plugin registration or `SDF_FORMAT_ARGS` parsing.
- Exposing laz-perf types. No laz-perf header appears on the public include
  path and no laz-perf type appears in a public signature.

## Public API

```text
usdlaz/Laz.h
```

| Group | Entry points |
| --- | --- |
| Codec seam | `LazDecoder` (`ReadHeader`, `ReadChunk`), `CreateFileDecoder` |
| Orchestration | `LazReader::Read`, `LazReader::ReadMetadata` |
| Aliases | `LazReadOptions` = `usdpointcloud::PointReadOptions`, `LazPointChunkConsumer`, `LazPointChunkErrorConsumer` |

`LazDecoder` is a public abstract interface, which makes the codec
substitutable and lets tests drive `LazReader` with a fake decoder and no
compressed input.

Minimal use:

```cpp
#include "usdlaz/Laz.h"

std::vector<usdgeo::Diagnostic> diagnostics;
auto decoder = usdlaz::CreateFileDecoder(path, diagnostics);
if (!decoder) {
    return false;
}

usdlaz::LazReader reader(std::move(decoder));
usdlas::LasHeader header;
usdpointcloud::PointData data;

const bool ok = reader.Read(
    usdlaz::LazReadOptions{},
    [&](const usdlas::LasHeader& h, const std::vector<usdlas::LasPoint>& points) {
        std::string error;
        return usdlas::AppendPointData(h, points, path, data, error);
    },
    header,
    diagnostics);
```

## Dependencies

`usdlas::core` and `usdpointcloud::core`, plus the vendored laz-perf sources
under `third_party/laz-perf/cpp/lazperf` compiled directly into this static
library. OpenUSD is **not** required.

Only the subset of laz-perf needed for the supported formats is compiled:
`charbuf`, `filestream`, `io`, `lazperf`, `vlr`, and the `field_*` detail
translation units. Omitted upstream components are listed in
`third_party/laz-perf/VENDORING.md`.

## Data flow

```text
.laz bytes
    | CreateFileDecoder -> LazDecoder (laz-perf)
    v
LazDecoder::ReadHeader   -> usdlas::LasHeader (compression bit cleared,
    |                       then validated by the shared LAS rules)
LazDecoder::ReadChunk    -> std::vector<usdlas::LasPoint>
    | LazReader, bounded by PointReadOptions
    v
consumer callback -> usdlas::AppendPointData -> usdpointcloud::PointData
    | usdlas::BuildPointCloudAsset
    v
usdpointcloud::PointCloudAsset
```

From `LasPoint` onward the path is identical to `usdLas`, which is what makes
LAS/LAZ output equivalence structural rather than maintained by hand.

## Error and diagnostic behavior

Nothing throws across the API boundary. Fallible entry points return `bool`
(or a null `unique_ptr` for `CreateFileDecoder`) and append
`usdgeo::Diagnostic` records to a caller-owned vector. Codec failures are
mapped onto the shared diagnostic codes, so a caller does not distinguish "the
codec failed" from "the file is malformed" by parsing a message.

`LazDecoder` supplies default `Diagnostic` overloads that adapt an
implementation providing only the `std::string& error` form, so a custom
decoder need implement just the string overloads.

Unsupported compressed formats — the waveform formats 4, 5, 9, and 10, which
the bundled codec has no record decoder for — are refused with a stable
diagnostic rather than decoded partially.

## Threading and ownership

`LazReader` **owns** its `LazDecoder` through a `std::unique_ptr` taken by the
constructor; the caller transfers ownership and must not retain a raw pointer
to it. Neither `LazReader` nor a `LazDecoder` is thread-safe: the codec holds
sequential stream state, so one reader must be driven from one thread.

The `std::vector<LasPoint>&` passed to a chunk consumer is **borrowed** and
valid only for the duration of the callback; it is reused for the next chunk.
Copy or move out anything that must outlive the call.

## Coordinate-space assumptions

Identical to `usdLas`: positions are `double` in **source** coordinates after
the header scale and offset, with no local-origin translation and no up-axis
conversion. Those happen in `usdPointCloudAuthoring`.

## Build and test

Builds and tests with plain CMake and **no OpenUSD runtime**:

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release -R usdLaz_unit --output-on-failure
```

The build fails fast with a clear message if `third_party/laz-perf` is
missing.

## Runtime and licensing

`laz-perf 2.0.0` is **LGPL-2.1** and is compiled into this static library,
which is then linked into the `pointcloud-laz` plugin shared library. Any
binary containing `usdLaz` therefore carries LGPL-2.1 obligations, including
the requirement to let a recipient relink against a modified laz-perf. The
`pointcloud-las` bundle contains no laz-perf code.

Read [DISTRIBUTION.md](../../docs/guides/DISTRIBUTION.md) before shipping a
binary built from this module. Changing the vendored revision requires updating
`third_party/laz-perf/VENDORING.md`, `THIRD_PARTY_NOTICES.md`, and the package
verification together.

## Known limitations

- Compressed point formats 0-3 and 6-8 only. Formats 4, 5, 9, and 10 are
  rejected because the bundled codec provides no compressed record decoder for
  them.
- Decoding is sequential: the codec exposes no compressed point seeking, so a
  point range avoids *delivering* unselected points but still *decodes* the
  chunks preceding the range.
- LAS 1.0 and 1.1 are rejected, as in `usdLas`.
- The vendored subset is fixed at build time; there is no option to link a
  system laz-perf shared library, which would simplify the LGPL obligation from
  section 6(a) to the shared-library case.
- Decoding assumes a little-endian host.

## Planned work

- Codec-specific diagnostics for the streaming path, and cross-format
  consistency tests asserting LAS and LAZ produce equivalent tiled output.
- Large-corpus bounded-memory measurement and failure-injection coverage for
  tiled payload generation.

See [streaming and tiling](../../docs/roadmap/streaming-and-tiling.md).

## Contracts

- [Workspace contract](../../docs/architecture/WORKSPACE.md)
- [Point reader architecture](../../docs/architecture/POINT_READER.md)
- [Diagnostics contract](../../docs/architecture/DIAGNOSTICS.md)
- [LAZ codec decision (ADR 0002)](../../docs/adr/0002-laz-codec.md)
- [Binary distribution and licensing](../../docs/guides/DISTRIBUTION.md)
