# OpenUSD Geospatial Ecosystem Strategy

Last updated: 2026-09-04

## Purpose

This document defines how the OpenUSD geospatial projects divide
responsibility and compose at runtime. It is the standing cross-repository
direction. Repository-specific implementation order remains in each
repository's roadmap; current capability remains in its reference documents.

The long-term goal is to use OpenUSD as a composition layer for large
geospatial data that can be accessed lazily, hierarchically, and through
independently distributed plugins and resolvers.

## Repository Boundaries

The ecosystem is split by data model and runtime responsibility:

```text
usd-pointcloud-plugins --\
usd-raster-plugins -----+--> runtime / distribution composition --> OpenUSD applications
usd-vector-plugins -----+                 ^
                        |                 |
usd-http-resolver ------+-----------------+
```

The repositories remain independently buildable and releasable. They do not
form one geospatial plugin monorepository because their format dependencies,
release cycles, and distribution constraints differ.

| Owner | Responsibilities | Does not own |
| --- | --- | --- |
| Format plugin repository | Parsing, source metadata, USD representation, and format-specific hierarchy or LOD adaptation | HTTP, cloud authentication, generic transport caching, or runtime distribution |
| Resolver repository | URL resolution, remote byte access, range requests, retry policy, validation tokens, and transport byte caches | Point-cloud, raster, or vector interpretation |
| Tool repository or tool layer | Conversion, inspection, validation, heightmap generation, and other explicit processing | Transparent asset resolution |
| Runtime composition repository | Plugin discovery, dependency composition, distribution, Python integration, and examples | File-format parsing |

Shared C++ libraries are extracted only after the same stable contract appears
in at least two or three projects. Plausible future shared concerns include
byte-source interfaces, coordinate transforms, spatial metadata, bounds,
cache vocabulary, and diagnostics; anticipated reuse alone is not sufficient.

## Project Direction

### Point clouds

`usd-pointcloud-plugins` is the reference implementation and remains focused
on point-cloud ingestion. LAS, LAZ, COPC, and PLY are the current priority
formats. Their stability, metadata preservation, bounded-memory processing,
and test coverage take precedence over increasing the format count.

COPC is the architectural reference for hierarchical and remote-capable point
clouds. Its hierarchy should map to coarse loading units such as regions,
tiles, LOD representations, and point chunks rather than one USD prim per COPC
node. Runtime transport remains resolver-owned.

Temporary generated USD is reduced where direct population, anonymous layers,
or in-memory layers are practical. Where temporary or generated files remain
necessary, their ownership, deterministic paths, crash recovery, and test
isolation must be explicit.

### Raster

Raster work belongs in `usd-raster-plugins`, with Cloud Optimized GeoTIFF as
the architectural reference for remote partial access and pyramidal detail.
GeoTIFF, COG, DEM, imagery, and heightmap inputs have dependencies and USD
representations distinct from point clouds. Derived operations such as point
cloud to heightmap or raster to mesh belong in explicit tools.

### Vector

Vector work belongs in `usd-vector-plugins`. The initial order is GeoJSON,
FlatGeobuf, GeoPackage, then Shapefile. FlatGeobuf is the reference candidate
for remote range access. Natural USD mappings are `UsdGeomPoints` for points,
`UsdGeomBasisCurves` for lines, and `UsdGeomMesh` for polygons, with source
attributes preserved as primvars where appropriate.

### Resolver

`usd-http-resolver` remains independent of every format repository and can
serve any OpenUSD asset. Remote COPC and COG require efficient range requests,
partial downloads, validation through ETag or equivalent tokens, retries, and
a byte-range cache. S3, GCS, and Azure Blob support may later use the same
transport abstraction without entering a format plugin.

The intended composition is:

```text
remote asset URL
       |
       v
OpenUSD ArResolver -> random-access asset -> format reader -> USD representation
```

### Runtime and distribution

A runtime composition repository is created only after point-cloud, raster,
vector, and resolver packages have stable independent contracts. It may provide
unified plugin discovery, optional Python packages, integration examples, and
distribution profiles. It does not implement file formats.

Python distribution should prefer optional components and avoid requiring a
second bundled OpenUSD runtime when a compatible installation already exists.
WebAssembly remains a later track, initially targeting parsers, decompression,
hierarchy inspection, and browser-side filtering rather than the complete
OpenUSD runtime.

## Shared Geospatial Rules

Source coordinates and spatial metadata are preserved. A plugin must not
silently reproject data. EPSG identifiers, projected or geographic CRS,
vertical datum, source origin, and local origin flow into metadata, while an
explicit transform layer owns reprojection. Local or floating origins are used
where large coordinates would exceed the useful precision of stage-local
values.

Small redistributable CC0 or public-domain fixtures belong in repositories.
Large real-world corpora are obtained through reproducible download scripts
and provenance records rather than Git LFS.

CI is divided by purpose: pull requests run minimal builds and focused tests,
nightly jobs run larger matrices and external corpora, and release jobs build
packages and run integration tests. OpenUSD and dependency build caches are
preferred over rebuilding the complete stack in every job.

## Delivery Order

Cross-repository work proceeds in this order:

1. Stabilize the point-cloud foundation, with COPC hierarchy, metadata,
   bounded memory, and tests as the primary targets.
2. Keep HTTP range access and caching in `usd-http-resolver`, and maintain a
   remote COPC end-to-end integration baseline.
3. Establish `usd-raster-plugins` around GeoTIFF, COG, and DEM, reusing the
   resolver through runtime composition.
4. Establish `usd-vector-plugins` around GeoJSON and FlatGeobuf.
5. Add a runtime composition and distribution repository after the three data
   model families have stable independent contracts.
6. Research host-driven LOD, asynchronous loading, spatial queries, Python
   distribution, and WebAssembly without making them near-term release gates.

For work inside this repository, the actionable sequence and completed work
remain in the [point-cloud roadmap](../roadmap/README.md). The implemented
surface remains in the
[capability matrix](../reference/CAPABILITY_MATRIX.md).