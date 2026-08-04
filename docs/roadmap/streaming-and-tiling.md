# Streaming and Spatial Tiling

This is the plan for the next major capability: consuming large point clouds
without accumulating the complete cloud in memory, and turning the result into
payload-backed `usdLod` tile assets.

Status: **in progress**. The `PointStream` contract, LAS and LAZ connections,
spill-backed routing, and one-tile-at-a-time payload authoring are implemented.
Generated-corpus RSS measurement and an explicit benchmark target are
implemented. Real-world datasets and payload working-set measurement remain
open. What `main` implements today is in
[implementation status](implementation-status.md) and
[capability matrix](../reference/CAPABILITY_MATRIX.md).

## 1. Problem statement

Chunked decoding reduces decoder buffer size. The tiled path additionally
spools points by source-coordinate tile and reconstructs one tile at a time
before writing its payload, so it does not retain the complete point cloud in
the authoring stage. A generated one-million-point corpus can be measured with
the explicit benchmark target; peak-memory measurement against large
real-world inputs is still outstanding.

The target pipeline must:

- avoid storing the complete point cloud in memory;
- preserve all selected point attributes;
- produce deterministic results;
- support cleanup after failures;
- preserve stable diagnostics;
- support LAS first and LAZ second;
- remain reusable by future COPC and E57 bundles.

## 2. Target pipeline

```text
LAS / LAZ reader
    | PointChunk
attribute selection and validation
    | PointChunk
spatial partitioning
    | tile spool
LOD sampling
    | tile level data
payload writer
    | tile assets
root usdLod authoring
```

The initial implementation favors correctness, deterministic output, and
bounded memory over sophisticated spatial indexing.

## 3. Streaming interface

A pull-based C++17 interface in `usdPointCloudCore` provides natural
backpressure and keeps buffer ownership explicit:

```cpp
class PointStream {
public:
    virtual ~PointStream() = default;

    virtual PointStreamStatus ReadNext(
        PointChunk& chunk,
        PointData& data,
        Diagnostic& diagnostic) = 0;
};
```

Reader-specific factory functions return this interface:

```cpp
std::unique_ptr<PointStream> OpenLasPointStream(...);
std::unique_ptr<PointStream> OpenLazPointStream(...);
```

`PointStream` is the second consumer of the existing `PointReadOptions`
contract, not a replacement for it; see
[point reader architecture](../architecture/POINT_READER.md).

## 4. Spill-backed fixed-grid tiling

```text
PointStream
    |
TileRouter
    |
per-tile memory buffers
    | threshold reached
temporary tile spool files
    | end of stream
LOD sampling and payload authoring
    |
root usdLod layer
```

Input points are not guaranteed to be ordered by tile, so keeping every tile
completely in memory would merely move the existing memory problem. External
sorting is powerful but unnecessarily complex for a first implementation.
Per-tile spool files are the practical first bounded-memory design.

The first version of `usdPointCloudTiling` uses:

- a fixed 2D grid;
- source horizontal coordinates;
- a fixed tile size;
- per-tile memory thresholds;
- temporary spool files;
- fixed-stride LOD samples;
- deterministic asset naming.

Complex octrees, adaptive density estimation, and COPC-specific hierarchy
optimization are deferred.

### Coordinate policy

Spatial partitioning uses source horizontal coordinates; OpenUSD authoring
continues to use stage-local coordinates. This keeps tile identity stable
across stage up-axis conversion and preserves the meaning of projected CRS
coordinates. See [ADR 0001](../adr/0001-coordinate-model.md).

## 5. Temporary spool requirements

The spool system must define:

- a binary schema version;
- the selected attribute layout;
- source and stage coordinate representation;
- file naming;
- flush thresholds;
- a maximum open file count;
- retry and failure behavior;
- cleanup behavior;
- detection of incomplete spools;
- deterministic iteration order.

Temporary output is isolated in a dedicated working directory and removed on
successful completion unless a debug-retention option is enabled.

## 6. Payload output requirements

The payload writer produces:

- one payload asset per tile and LOD level, or another documented
  deterministic grouping;
- relative and portable asset paths;
- stable prim paths;
- root `usdLod` metadata;
- a source fingerprint and the generation settings;
- tile bounds and point counts;
- no partially published root asset after failure.

Final output is committed atomically where the filesystem allows it. The
authored representation stays inside the existing
[tile and LOD contract](../architecture/LOD.md); streaming changes how assets
are produced, not what they look like.

## 7. Initial FileFormat arguments

The existing compact `lod` profiles remain. Spatial streaming is introduced
with a small, explicit argument set:

```text
tile=true
tileSize=<source units>
tileMemoryLimit=<bytes or profile>
payloadDirectory=<relative or generated path>
```

Not every internal tuning parameter is exposed in the first public contract.
Profiles map to internal defaults; explicit advanced arguments are added only
after real data testing. Argument normalization, layer identity, and cache-key
participation follow the existing
[file-format argument contract](../architecture/FILE_FORMAT_ARGUMENTS.md).

## 8. Out of scope for the first streaming release

- adaptive octrees;
- density-aware tile sizing;
- multithreaded decode and authoring;
- distributed processing;
- COPC hierarchy preservation;
- renderer-specific page scheduling;
- automatic cache eviction.

## 9. Implementation sequence

Each step is one pull request. Behavior changes are not mixed into the rename
and documentation steps that precede them.

| Step | Title | Scope |
| --- | --- | --- |
| 1 | Rename point-cloud authoring and geospatial plugin modules | Done: directory, bundle, CMake target, manifest, `plugInfo.json`, CI, and test renames with no behavior change. |
| 2 | Synchronize documentation with current implementation | Done: capability status, unreleased change record, renamed references, and a `README.md` for every module. |
| 3 | Add point-stream and spatial-tiling contracts | `PointStream`; tile keys and configuration; tile router; spool interface and schema; the deterministic output contract; tests that run without an OpenUSD runtime; `libs/usd-pointcloud-tiling/README.md`. The FileFormat Plugins are not connected yet. |
| 4 | Connect LAS streaming to tiled payload authoring | LAS `PointStream`; source-coordinate tile routing; spool generation; tile payload output; root `usdLod` output; bounded-memory tests; cleanup and failure tests. LAS is first so tiling issues are not mixed with LAZ codec issues. |
| 5 | Connect LAZ streaming to tiled payload authoring | LAZ `PointStream`; an output contract equivalent to LAS; codec-specific diagnostics; cross-format consistency tests. |
| 6 | Enable spatial tiled reads through FileFormat arguments | Enable the spatial tile argument; define output and cache paths; connect profiles to tiling configuration; update plugin READMEs and root documentation; add integration tests with plugin discovery. |

## 10. Testing requirements

### Unit tests

Each module owns tests for its own contract:

- `usdPointCloudCore`: stream ownership and validation;
- `usdPointCloudTiling`: tile keys, negative coordinates, boundaries,
  deterministic ordering, spill thresholds;
- `usdPointCloudAuthoring`: asset paths, prim paths, LOD metadata, payload
  round trips;
- `usdLas`: chunk continuity and attribute preservation;
- `usdLaz`: codec chunk continuity and error mapping.

### Integration tests

- LAS direct non-tiled read;
- LAZ direct non-tiled read;
- metadata-only read;
- tiled LAS output;
- tiled LAZ output;
- payload paths after moving the output directory;
- interrupted generation cleanup;
- empty point cloud;
- negative tile coordinates;
- invalid Extra Bytes names;
- point attributes preserved across spool round trips;
- deterministic output from repeated runs.

### Memory tests

At least one generated large-corpus test verifies that peak buffering is
bounded by configuration rather than by total point count. CI does not need a
truly massive file, but the test must generate enough points and tiles to force
repeated spill and flush behavior.

### Benchmarks

Track at least:

```text
LAS decode points per second
LAZ decode points per second
peak resident memory
spool write volume
payload authoring time
tile count
root layer size
time to first usable LOD asset
```

Benchmarks may initially run outside required PR CI, but their commands and
datasets must be documented.

## 11. Definition of done

The first streaming phase is complete when:

- LAS and LAZ can be consumed through `PointStream`;
- point data is partitioned without accumulating the complete cloud;
- tile buffers spill at a configured threshold;
- payload-backed tile assets are generated deterministically;
- a root `usdLod` asset references the generated payloads;
- selected attributes survive the full pipeline;
- failures do not leave a valid-looking partial root asset;
- temporary files are cleaned up;
- memory use is bounded by tile and spool configuration;
- direct FileFormat arguments can enable the path;
- all affected module READMEs and architecture documents are updated.

## 12. Related open contracts

Extra Bytes descriptor names are currently rejected when they are not valid USD
identifiers, which fails whole files for names such as `temperature (C)` or
`Height above ground`. The normalization contract — invalid characters replaced
with `_`, a prefix for names beginning with a digit, deterministic collision
suffixes, the original descriptor name retained as metadata, and an optional
strict mode — must be defined before Extra Bytes support is widened further. It
is independent of streaming and can land in any order relative to steps 3-6.
