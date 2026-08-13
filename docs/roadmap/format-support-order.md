# File Format Support Order

## Selection Criteria

Formats are prioritized by their ability to validate the shared architecture,
deliver practical value, and reuse earlier work. This is the order among format
additions, not the release order: source identity, cache hardening, and adaptive
tiling take priority after resolver-backed COPC. A format moves into active
implementation only when its required foundation and test data are available.

Priority is determined by:

1. Value for surveying, scanning, mapping, and 3D data-exchange point-cloud workflows
2. Ability to validate direct OpenUSD FileFormat Plugin access
3. Reuse of existing readers, metadata contracts, tiling, and cache infrastructure
4. Availability and licensing of portable dependencies
5. Partial-read and spatial-index capabilities
6. Complexity of preserving source metadata without loss

## Support Sequence

Infrastructure gates before the next format addition:

| Order | Capability | Release target | Exit gate |
| --- | --- | --- | --- |
| 1 | Resolver-backed COPC | `v0.5.0` | Project-owned random access, `ArAsset` adaptation, selective reads, and conservative identity are verified |
| 2 | Source identity and cache hardening | `v0.6.0` | Local and remote generated-output reuse has explicit invalidation and recovery rules |
| 3 | Point-budget-aware adaptive tiling | `v0.7.0` | Deterministic planning, memory limits, tile statistics, and cross-format baselines are available |
| 4 | Measurement and I/O observability | `v0.8.0` | Fixed-grid and adaptive are compared on uneven real data, and source, spool, and payload I/O are visible in benchmark output; host responsiveness moves to `v0.9.0` |
| 5 | TilePlan convergence and interactive validation | `v0.9.0` | Sequential planning and COPC native hierarchy reach payload authoring through one plan representation, with a reproducible host-responsiveness baseline |
| 6 | Resolver-backed source identity | `v0.10.0` | Generated-cache reuse is enabled exactly where a resolver supplies sufficient identity, with transport still owned by the resolver |

The format sequence below resumes only after those infrastructure gates. E57
is the preferred substantial format extension; delimited text and other
point-cloud formats remain candidates whose order may be driven by concrete
workflows and test data.

Gate 5 is the one that most affects later formats. Once a tile plan is a single
representation, a new format supplies points and, where it has one, a native
hierarchy; tiling, filtering, sampling, caching, and authoring are reused
rather than re-implemented. The intended responsibility of a new format
shrinks to:

```text
new format -> PointStream / RandomAccessSource
```

| Order | Format | Initial scope | Why this position | Entry gate |
| --- | --- | --- | --- | --- |
| 1 | LAS | LAS 1.2-1.4 headers, point formats 0-3 and 6-8, VLR/EVLR, XYZ, intensity, returns, classification, RGB, GPS time, CRS; then the full LAS 1.4 attribute set, Extra Bytes, and formats 4, 5, 9, 10 | Uncompressed records expose the complete coordinate and point-attribute model without codec complexity | `usdGeoCore`, `usdPointCloudCore`, and a minimal `usdPointCloudAuthoring` writer are tested |
| 2 | LAZ | The LAS logical model with chunked decompression and the same file-format arguments | Delivers the common production format while reusing LAS semantics | LAS conformance corpus passes and the codec decision is recorded |
| 3 | COPC | Read existing hierarchy, bounds, chunks, and LOD metadata; no writer initially | Validates native spatial hierarchy and partial loading before inventing a repository-specific point-cloud container | Stable LAZ chunk reading and tile contracts exist |
| 4 | PLY | Binary and ASCII vertex elements, arbitrary per-vertex properties, XYZ, normals, RGB, intensity; CRS and units from file-format arguments; face elements deferred | Smallest self-describing point container; proves the generic attribute mapping is not LAS-specific | Generic point attributes and deterministic primvar naming are stable |
| 5 | XYZ / PTS / CSV | Delimiter and column mapping, optional header line, unit and CRS arguments, line-anchored diagnostics, bounded streaming | Exercises normalized file-format arguments and cache keys harder than any binary format, and is ubiquitous in survey exchange | File-format argument normalization, cache-key inputs, and the streaming reader API exist |
| 6 | E57 | Multiple `Data3D` scans, per-scan pose, cartesian and spherical coordinates, intensity, RGB, timestamps, per-scan bounds; `Image2D` deferred | High-value survey format whose multi-scan model is a real contract extension rather than another LAS variant | Point-cloud contracts carry multiple scans and per-scan transforms without LAS-specific assumptions |

The sequence keeps the point-cloud family contiguous, so the shared point
schema evolves once instead of being revisited after unrelated work. LAS,
LAZ, COPC, and PLY established the current generic attribute and streaming
model. Later formats must reuse that model; E57 may extend it for multiple
scans only after the infrastructure roadmap is complete.

Formats without embedded georeferencing (PLY, XYZ, PTS, CSV, and some E57
files) never guess a CRS or unit. Missing georeferencing is reported as a
diagnostic and can only be supplied explicitly through file-format arguments.

Every added reader is held to the same boundaries:

- USD authoring does not enter the reader;
- tiling does not enter the reader;
- caching does not enter the reader;
- an OpenUSD dependency does not enter the reader where it can be avoided;
- the reader connects through the `PointStream` or `RandomAccessSource`
  contract.

XYZ, PTS, PCD, and similar formats have comparatively light readers, which
makes them useful for checking that the shared pipeline is genuinely generic
rather than LAS-shaped. E57 remains the higher-value extension because scanner
and survey workflows depend on it.

Terrain, raster, and vector formats are intentionally excluded from this
sequence. They are future repository candidates with different storage,
partial-read, and authoring contracts.

## Delivery Stages Per Format

Every format progresses through the same stages:

1. **Inspect**: parse metadata and report unsupported features without authoring a stage.
2. **Decode**: produce validated project-owned intermediate data.
3. **Direct read**: open the source as an OpenUSD layer.
4. **Selective read**: support normalized attributes, bounds, LOD, or layer arguments where applicable.
5. **Cache**: generate and reuse deterministic USDC cache content.
6. **Scale**: add chunking, tiling, parallelism, and recovery for large or damaged data.

A later stage must not delay a correct implementation of an earlier stage. Write-back support is a separate future decision and is not part of initial format completion.

## Point-Cloud Milestones

### Milestone A: LAS Preview

- Open `.las` directly as an OpenUSD layer.
- Author XYZ and available RGB or intensity as `UsdGeomPoints`.
- Preserve CRS, Scale, Offset, bounds, and source point-format metadata.
- Reconstruct source coordinates within the documented error budget.

### Milestone B: LAZ Production Read

- Open `.laz` through the same logical model and argument contract as LAS.
- Decode incrementally rather than materializing the compressed source at once.
- Select attributes and enforce deterministic point limits.
- Reuse USDC cache content across equivalent normalized arguments.

### Milestone C: Tiled Point Clouds

- Represent source or generated spatial hierarchy through shared tile contracts.
- Read only required COPC or generated tiles.
- Provide two or three deterministic nested LOD levels per tile, ordered from
  highest to lowest detail.
- Author each tile as an OpenUSD 26.08 LOD root with a referenced screen-size
  heuristic and a default index, and leave selection to the host.
- Measure stage population and payload behavior instead of inferring it from
  render visibility.

The public representation and its invariants are fixed in the
[tile and LOD contract](../architecture/LOD.md).

COPC-specific structures stay out of `usd-laz`. They live either in a separate
`usd-copc` library or in an isolated COPC module inside `usd-laz`, and their
hierarchy, octree keys, byte ranges, and resolution map onto the shared tile
contract.

### Milestone D: Generic Point Containers

- Open `.ply` and delimited text point files through the same reader and
  authoring path as LAS.
- Map arbitrary per-vertex or per-column properties onto the generic point
  attribute model, with deterministic primvar names.
- Require explicit CRS, unit, and column arguments instead of guessing, and
  report their absence as a diagnostic.
- Keep parse failures anchored to a byte offset or a line number.

### Milestone E: Multi-Scan Point Clouds

- Read an E57 file containing several `Data3D` scans in one pass.
- Represent per-scan pose, bounds, and attribute availability through the
  shared point-cloud contracts, without LAS-specific assumptions.
- Author scans as separate prims under a shared georeferenced parent, so a
  single scan can be loaded independently.
- Convert spherical coordinates and per-scan transforms with the same
  precision budget as LAS.

## Future Repository Candidates

The following are not release gates for this repository:

- `usd-terrain-plugins`: GeoTIFF elevation, DEM, COG, heightmaps, and terrain meshes
- `usd-vector-plugins`: GeoJSON, FlatGeobuf, GeoPackage, and Shapefile

## Deferred Formats and Features

The following remain deferred until a concrete workflow and ownership boundary are established:

- Full LAS / LAZ write-back and round-trip guarantees
- Waveform sample data interpretation beyond packet metadata and external
  packet references
- E57 image extraction and advanced scan reconstruction
- Arbitrary raster imagery and multidimensional GDAL datasets
- Full GeoPackage raster support
- 3D Tiles terrain and tileset ingestion
- Proprietary vendor VLR interpretation
- Renderer-specific point primitives or shaders
