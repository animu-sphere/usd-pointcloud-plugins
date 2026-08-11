# Infrastructure Maturity Roadmap

## Direction

`usd-pointcloud-plugins` is evolving from a collection of LAS-family importers
into format-independent OpenUSD point-cloud ingestion infrastructure:

```text
format decoder / adapter
          |
          v
      PointStream
          |
          +-- metadata, ranges, filtering, cancellation
          +-- bounded-memory chunks
          v
 tiling / LOD / cache
          |
          v
 OpenUSD authoring
```

The next releases prioritize depth over format count. New formats adapt to the
shared `PointStream`, processing, cache, and authoring contracts rather than
reimplement them.

## Scope

In scope are point-cloud readers, metadata extraction, CRS and spatial
metadata, filtering, bounded streaming, LOD, tiling, payload generation,
generated-USDC caching, resolver-backed range access, OpenUSD FileFormat
integration, and explicit conversion workflows.

Point-cloud rendering, viewport LOD selection, PLY mesh import, generic mesh
import, raster and terrain data, vector GIS, point-cloud writing, and a generic
scene-conversion framework are out of scope. A public custom USD point-cloud
schema is deferred until the plain-attribute metadata contract is stable.

## Operating Principles

- Keep format parsing independent of OpenUSD and transport policy.
- Preserve `PointStream` as the format-independent reader boundary.
- Keep spatial tiling separate from LOD selection.
- Keep generated-USDC cache identity independent of format and transport.
- Treat FileFormat reads as interactive inspection and preview paths.
- Treat `usd-pointcloud-convert` as the production path for long-running,
  deterministic, payload-backed generation and cache population.
- Prefer OpenUSD asset resolution over format-specific HTTP, cloud, or
  authentication clients.
- Keep fixed-grid tiling until a deterministic point-budget planner is proven.
- Add a new format only when it strengthens or cleanly reuses the common
  infrastructure.

## Release Sequence

| Release | Theme | Primary outcome | Status |
| --- | --- | --- | --- |
| `v0.4.0` | PLY read support | Prove the shared contracts outside the LAS family | Released |
| `v0.5.0` | Remote source architecture | Resolver-backed COPC through a project-owned random-access source | Released 2026-08-12 |
| `v0.6.0` | Cache and source identity | Deterministic local reuse and conservative remote identity | Released 2026-08-12 |
| `v0.7.0` | Adaptive tiling | Predictable payload density and memory through point-budget planning | Planned |
| Later | Format expansion | E57 and other point-cloud formats through `PointStream` | Deferred |

### `v0.5.0` - Remote Source Architecture

The resolver-backed COPC implementation is on `main`. `usdCopc` consumes a
project-owned random-access byte source and remains independent of OpenUSD,
HTTP, and cloud APIs. The plugin layer adapts resolver-opened `ArAsset` values:

```text
local file -> LocalRandomAccessSource --\
                                          +-> usdCopc
ArResolver -> ArAsset source adapter ----/
```

The release is complete when tests demonstrate selective header, hierarchy,
and point-data reads; equivalent local and resolver-backed output; bounded
memory; cancellation and deterministic diagnostics; and conservative cache
behavior when a resolver cannot provide stable identity.

This release does not add a generic HTTP client, S3 SDK, authentication flow,
network retry subsystem, adaptive tiling, E57, or COPC writing. The included
HTTP resolver is an integration-test double, not a production network
transport.

### `v0.6.0` - Cache and Source Identity

Primary goal: make local and remote access deterministic and reusable without
leaking transport-specific policy into `usdGeoCache`.

Candidate work:

- define a formal source identity contract; the initial neutral value contract
  is implemented by `usdgeo::cache::SourceIdentity`, and local filesystem
  identity construction is shared by authoring and conversion;
- document cache invalidation and compatibility rules;
- accept stable resolver metadata such as resolved identifiers, sizes,
  validation tokens, modification times, or digests through a neutral source
  identity value;
- investigate partial or range-cache ownership without conflating it with the
  generated-USDC cache;
- add cache statistics and diagnostics;
- improve corruption and interrupted-publication recovery;
- measure cache hit ratios for representative local and remote workflows.

Cache reuse remains disabled when source identity is not stable enough to
exclude stale output.

The current baseline is explicit: local conversion records one miss followed
by one hit for the same descriptor, while resolver-backed COPC remains a
non-reuse path unless the resolver can supply a stable validation token. The
COPC integration test keeps this conservative behavior covered when
`USDGEO_CACHE_ROOT` is configured.

### `v0.7.0` - Adaptive Tiling

Primary goal: make payload density, payload size, and peak memory more
predictable across uneven datasets.

Candidate work:

- deterministic point-budget planning;
- `maxPointsPerTile`, `minPointsPerTile`, and `maxDepth` controls;
- optional target payload-byte and spatial-size fallbacks;
- tile statistics and planning diagnostics;
- LAS, LAZ, COPC, and PLY comparison baselines.

The existing `tileSize` and `tileMemoryLimit` fixed-grid path remains supported
because it is simple and predictable. Adaptive planning follows remote source
and cache identity work; it does not block them.

### Later - E57 and Other Point-Cloud Formats

E57 is valuable but is not the immediate next milestone. Its decoder should
enter the existing pipeline as an adapter:

```text
E57 decoder -> PointStream -> filters / tiling / authoring / cache
```

If multi-scan E57 data requires a change below `PointStream`, that need is
evidence about the common contract, not permission to build a parallel
authoring path. Delimited text and other point-cloud formats are also deferred
until the infrastructure milestones above are mature.

## Public Contracts

The current `geo:*` metadata and point attributes are becoming a public data
contract. Their names, USD types, meaning, units, coordinate spaces, source
mappings, required status, and stability belong in a canonical point-cloud
metadata reference. A custom USD schema remains deferred until this contract
is stable across formats.

## Testing Priorities

Architecture-level tests accompany correctness tests:

- equivalent positions, attributes, metadata, filters, and LOD output across
  equivalent LAS, LAZ, and COPC sources;
- chunk limits, memory budgets, cancellation, truncated input, and partial
  range behavior;
- exact and overlapping remote reads, short reads, missing ranges, resolver
  failures, cancellation, and changing source identity;
- deterministic tile IDs and payload paths, correct bounds and LOD extents,
  and cleanup after failure;
- cache hit, miss, stale source, corrupt entry, incompatible version, and
  interrupted-write behavior.

## Performance Baselines

Representative measurements record source size, point count, peak RSS, spool
size, output size, payload count, conversion time, range-read count, bytes
fetched, and cache hit ratio. For remote COPC, `bytes fetched / source size` is
a primary KPI: useful remote access must fetch only the required ranges.

## Guiding Question

> Does this make `usd-pointcloud-plugins` better OpenUSD point-cloud
> infrastructure, or does it merely add another importer?

Prefer the former. Releases v0.1 through v0.4 established format capability;
v0.5 through v0.7 prioritize infrastructure maturity before format expansion.