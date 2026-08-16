# Infrastructure Maturity Roadmap

## Direction

`usd-pointcloud-plugins` is no longer a collection of LAS-family importers. It
is format-independent OpenUSD point-cloud ingestion infrastructure whose
current assets are a source abstraction, a bounded-memory stream contract,
format-independent filtering and sampling, fixed-grid and adaptive tiling, a
deterministic generated-output cache, payload-backed authoring, typed
diagnostics, and a reader layer that does not depend on OpenUSD:

```text
Source / RandomAccessSource
          |
          v
      PointStream
          |
          +-- metadata, ranges, filtering, cancellation
          +-- bounded-memory chunks
          v
 filtering / sampling
          |
          v
 tiling: fixed-grid / adaptive
          |
          v
   point-cloud authoring
          |
          v
  FileFormat / Converter
```

The long-term target is not "OpenUSD plugins for point-cloud formats". It is a
format-independent point-cloud ingestion, streaming, tiling, caching, and
OpenUSD authoring framework:

```text
                        +-- LAS
                        +-- LAZ
 source abstraction ----+-- PLY
                        +-- E57
                        +-- ...
                             |
                             v
                        PointStream
                             |
                +------------+------------+
                v                         v
        sequential planner         native hierarchy
                |                     (COPC etc.)
                +------------+------------+
                             v
                          TilePlan
                             |
                    filtering / sampling
                             |
                             v
                     payload authoring
                             |
                  +----------+----------+
                  v                     v
             FileFormat             Converter
```

The next releases prioritize depth over format count. Adding a format should
eventually mean connecting a decoder to `PointStream` or `RandomAccessSource`
and reusing tiling, cache, filtering, LOD, and authoring unchanged.

## Two Kinds of Streaming

Streaming is not one problem. The two cases have different owners, different
success criteria, and different risks, and they are not merged.

### Conversion streaming

```text
large input -> bounded-memory processing -> USD payload generation
```

Its goals are memory use independent of input size, safe long-running
processing, recoverable intermediate state after failure or cancellation, and
deterministic output. This is the mature part of the project and the part the
next releases finish.

### Runtime streaming

```text
camera / host LOD decision -> required tile -> range request -> decode -> render
```

Its goals belong to composition, payload loading, the host application, and the
renderer. A FileFormat Plugin cannot own that decision alone. Runtime streaming
is therefore a research track, not a release gate, and it must not introduce
premature abstractions into the conversion pipeline.

## Scope

In scope are point-cloud readers, metadata extraction, CRS and spatial
metadata, filtering, bounded streaming, LOD, tiling, tile planning, payload
generation, generated-USDC caching, resolver-backed range access, OpenUSD
FileFormat integration, and explicit conversion workflows.

Point-cloud rendering, viewport LOD selection, PLY mesh import, generic mesh
import, raster and terrain data, vector GIS, point-cloud writing, and a generic
scene-conversion framework are out of scope. A public custom USD point-cloud
schema is deferred until the plain-attribute metadata contract is stable.

## Operating Principles

- Keep format parsing independent of OpenUSD and transport policy.
- Preserve `PointStream` as the format-independent reader boundary.
- Keep spatial tiling separate from LOD selection.
- Keep conversion streaming and runtime streaming separate problems.
- Keep generated-USDC cache identity independent of format and transport.
- Treat FileFormat reads as interactive inspection and preview paths.
- Treat `usd-pointcloud-convert` as the production path for long-running,
  deterministic, payload-backed generation and cache population.
- Prefer OpenUSD asset resolution over format-specific HTTP, cloud, or
  authentication clients.
- Compose external resolvers at runtime; never depend on one at build time.
- Keep source identity transport-neutral, and treat resolver validation tokens
  as opaque.
- Keep fixed-grid tiling as the simple, predictable compatibility path.
- Evaluate designs against both memory bounds and I/O bounds.
- Change the spool design only after measurement shows it is the bottleneck.
- Do not force a source with a native hierarchy through sequential planning
  when its own structure already answers the question.
- Add a new format only when it strengthens or cleanly reuses the common
  infrastructure.

## Release Sequence

| Release | Theme | Primary outcome | Status |
| --- | --- | --- | --- |
| `v0.4.0` | PLY read support | Prove the shared contracts outside the LAS family | Released |
| `v0.5.0` | Remote source architecture | Resolver-backed COPC through a project-owned random-access source | Released 2026-08-12 |
| `v0.6.0` | Cache and source identity | Deterministic local reuse and conservative remote identity | Released 2026-08-12 |
| `v0.7.0` | Adaptive tiling | Predictable payload density and memory through point-budget planning | Released 2026-08-13 |
| `v0.8.0` | Measurement and I/O observability | Real-world adaptive baselines and visible I/O amplification | Released 2026-08-14 |
| `v0.9.0` | TilePlan convergence and interactive validation | One tile-plan representation for sequential planning and COPC native hierarchy, plus a host-responsiveness baseline | Released 2026-08-15 |
| `v0.10.0` | Resolver-backed source identity and external resolver interoperability | Safe generated-cache reuse for resolver-provided sources, with transport owned by the resolver | Planned |
| Research | Runtime streaming | Evidence about host-driven partial loading, with no premature abstraction | Ongoing |
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

Delivered:

- a formal source identity contract implemented by
  `usdgeo::cache::SourceIdentity`, with local filesystem identity construction
  shared by authoring and conversion;
- documented cache invalidation and compatibility rules;
- acceptance of stable resolver metadata such as resolved identifiers, sizes,
  validation tokens, modification times, or digests through a neutral value;
- cache statistics and diagnostics;
- corruption and interrupted-publication recovery.

Cache reuse remains disabled when source identity is not stable enough to
exclude stale output. The current baseline is explicit: local conversion
records one miss followed by one hit for the same descriptor, while
resolver-backed COPC remains a non-reuse path unless the resolver can supply a
stable validation token. The COPC integration test keeps this conservative
behavior covered when `USDGEO_CACHE_ROOT` is configured. Range-cache ownership
and remote hit-ratio measurement carry into `v0.10.0`.

### `v0.7.0` - Adaptive Tiling (released 2026-08-13)

Primary goal: make payload density, payload size, and peak memory more
predictable across uneven datasets.

Delivered:

- deterministic point-budget planning;
- `maxPointsPerTile`, `minPointsPerTile`, and `maxDepth` controls;
- tile statistics and planning diagnostics;
- LAS, LAZ, COPC, and PLY fixture-based comparison baselines.

The existing `tileSize` and `tileMemoryLimit` fixed-grid path remains supported
because it is simple and predictable. Broader real-world baselines and target
payload-byte or spatial-size fallbacks are the subject of `v0.8.0`.

### `v0.8.0` - Measurement and I/O Observability

Primary goal: replace assumptions about adaptive tiling and about the spool
with measurements on real data.

Adaptive planning is implemented and covered by fixtures, but fixtures cannot
show what uneven real-world density does to tile distribution, payload size,
or peak memory. Until that is measured, further tiling design is speculation.

The planning model whose behavior is measured is:

```text
pass 1: bounds and statistics
pass 2: cell counts and planning
            |
            v
   deterministic tile plan
            |
            v
     payload authoring
```

The tile plan being fixed before payload authoring is the property that later
work depends on, because it is what makes spool volume and I/O predictable in
advance.

Scope:

- compare fixed-grid and adaptive output on the same real-world inputs;
- extend the streaming benchmark with I/O counters, not only memory counters;
- record the results as reproducible baselines with documented commands.

Comparison metrics:

| Metric | Why it is recorded |
| --- | --- |
| Points per tile distribution | Whether the point budget is actually held |
| Payload bytes per tile | Whether density maps to asset size |
| Total payload bytes | Cost of the generated asset set |
| Tile count | Composition and file-count pressure |
| Tree depth | Planner behavior on uneven density |
| Peak RSS | Memory bound |
| Spool bytes | Intermediate write volume |
| Source read bytes | Read amplification |
| Total processing time | Practical conversion cost |
| `usdview` open time | Consumption cost |

I/O observability adds source bytes read, spool bytes written, spool bytes
read, payload bytes written, and effective I/O amplification to the benchmark
output. Conceptually, a large conversion moves data five times:

```text
source read -> decoded point traffic -> spool write -> spool read
                                                    -> payload write
```

A bounded-memory design that is not I/O bounded is only half solved. The
project therefore evaluates both, and the spool is not redesigned or removed
on suspicion. It is measured first, and changed only where it is proven to be
the bottleneck.

Real-world validation uses inputs with genuinely uneven density rather than
synthetic fixtures alone:

```text
forest         high density
buildings      high / medium density
roads          medium density
open terrain   low density
```

Datasets that cannot be redistributed are referenced through corpus
provenance records with reproducible download and run commands, following the
existing Shizuoka and USGS 3DEP practice.

Exit gate: fixed-grid and adaptive baselines for real LAS, LAZ, COPC, and PLY
inputs are published with the metrics above, and I/O amplification is visible
in benchmark output.

### `v0.9.0` - TilePlan Convergence and Interactive Validation

Primary goal: let sequential formats and natively hierarchical formats produce
the same tile-plan representation, so everything downstream stops caring which
planner ran.

LAS, LAZ, and PLY must scan the input to decide how to partition it. COPC
already carries a hierarchy, and recomputing it is wasted work:

```text
LAS / LAZ / PLY -> AdaptivePlanner        --\
                                              +-> TilePlan
COPC native hierarchy -> CopcHierarchyPlanner-/
```

With one intermediate representation, authoring, cache identity, and payload
generation are written once against `TilePlan` instead of once per planner.
This milestone also carries forward the host-responsiveness measurement that
was not required to close the v0.8.0 I/O gate.

Scope:

- define the `TilePlan` contract first: tile identity, bounds, point counts,
  parent and child relationships, source ranges, depth, and the planner
  identity and version that produced it;
- state how cache identity is derived from a plan, so two planners that
  describe the same partition are not forced to produce different keys by
  accident;
- move the existing adaptive planner onto the contract without changing its
  output;
- implement the COPC fast path that maps native hierarchy nodes and byte
  ranges onto a plan instead of re-deriving them.
- measure host responsiveness while interacting with generated output, using
  a documented workload and the reproducible payload-backed fixture.

Contract work precedes implementation. If the COPC hierarchy cannot be
expressed as a plan without distorting it, that is a finding about the
contract, not a reason to add a second authoring path.

Exit gate: adaptive and COPC-native plans reach payload authoring through one
representation, the COPC path demonstrably avoids the sequential planning
passes, authored output equivalence is covered by tests, and a reproducible
host-responsiveness baseline is recorded.

#### Host-responsiveness baseline (2026-08-15)

The interactive workload uses the reproducible USGS 3DEP 4,096-point,
payload-backed fixture. It sends `Home`, `Left`, `Right`, `Up`, and `Down` at
500 ms intervals while sampling the complete OST process tree every 50 ms.
For each action, the harness synchronously dispatches `WM_NULL` with a bounded
timeout to measure UI-thread responsiveness, then checks that the key-down and
key-up messages were queued successfully:

```powershell
python tools/measure_usd_working_set.py `
  --bundle .\plugins\pointcloud-las `
  --with-bundle .\plugins\pointcloud-laz `
  --fixture .\build\usdview-fixture-usgs-4096-interactive\PointCloud.usda `
  --mode interactive --renderer Storm `
  --interaction-interval-ms 500 --interval-ms 50 `
  --post-interaction-seconds 3
```

The local run used the pinned `cy2026` / `usd` OST environment and reported:

| Fixture | Actions | Max UI dispatch | Stage open | Peak tree working set | Peak process working set | Samples |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| USGS 3DEP 4,096-point payload fixture | 5 | 0.132723 s | 0.027894 s | 316,616,704 B | 296,374,272 B | 48 |

The harness returned success after the workload completed and usdview exited
normally. The baseline measures bounded UI-thread dispatch latency and
working-set pressure. It does not measure renderer frame latency or a
host-specific input-to-present time; queued key messages are validated
separately from the synchronous UI dispatch probe.

### `v0.10.0` - Resolver-Backed Source Identity

Primary goal: make generated point-cloud assets safely reusable across
resolver-backed sources without depending on any transport implementation.

Local filesystem identity is settled. Resolver-backed sources are currently
excluded from reuse whenever a stable validation token is absent, which is
correct but conservative enough to make remote workflows recompute output they
already have. The release closes that gap from the identity side only. The
binding contract is in the
[resolver-backed source contract](../architecture/RESOLVER_SOURCE.md).

Five outcomes define the release:

1. a stable resolver-backed source identity contract;
2. safe generated-cache reuse for resolver-provided sources;
3. conservative fallback when a resolver cannot provide stable identity;
4. removal of transport-specific HTTP ownership from this repository;
5. reproducible interoperability tests proving an external resolver can satisfy
   the point-cloud contracts.

#### Identity contract

Source identity describes what content was resolved, not how it was
transported. `resolvedIdentifier`, `size`, and an opaque `validationToken` are
the neutral representation; `url` and `etag` are not. A resolver may derive the
token from an HTTP `ETag`, `Last-Modified` plus size, an object version id, a
generation number, filesystem metadata, a digest, or a studio revision, and
none of those concepts enter the cache contract.

Identity is classified before it is trusted:

```text
Stable        sufficient for safe generated-cache reuse
Unstable      readable source, freshness not guaranteed
Unavailable   no usable identity metadata
```

Reuse stays disabled for `Unstable` and `Unavailable`, preserving the fail-safe
behavior established before this release.

One resolver-facing adapter translates `ArResolver` / `ArAsset` identity into
`SourceIdentity`. It lives outside format readers — LAS, LAZ, COPC, and PLY do
not each extract identity — and is owned by `usdGeoCache` or a small resolver
integration layer adjacent to it, so a future raster or vector repository can
reuse it. Point-cloud tiling concepts do not move into `usdGeoCore` for this.

#### Cache behavior

Raw byte and range caching belongs to the resolver; the generated USDC and
payload cache belongs here. A resolver-backed key adds stable source identity
to the deterministic generation inputs already used for local sources,
including `TilePlan` planner identity and version from `v0.9.0`. Reuse requires
stable identity, a matching descriptor, matching generation and `TilePlan`
inputs, and a validating root and payload set; anything else fails closed.

The central correctness property is that the same logical identifier with a
different validation token must not hit an existing entry.

Credentials, authorization headers, signed URLs, and tokens are never persisted
into manifests, cache descriptors, or diagnostics.

#### Repository boundary

The boundary does not move: byte-range access, COPC metadata and hierarchy,
point decoding, typed diagnostics, source identity, and cache compatibility
belong here; HTTP, authentication, retry, redirect, credentials, network
caching, and transport behavior belong to the resolver. The repository must
build and test with no CMake dependency, submodule, vendored HTTP library,
resolver-specific include, or link dependency on any resolver implementation.
`usd-http-resolver` is documented as one compatible implementation composed at
runtime through `PXR_PLUGINPATH_NAME`, never as a requirement.

`plugins/httpresolver` stops being presented as part of the product surface. It
is removed once equivalent external integration coverage exists, or relocated
under an explicitly test-only path such as `tests/plugins/httpresolver/`.

#### Tests

Tier 1 runs without any external resolver repository, using fake or
memory-backed test assets, and remains the required CI gate: resolver-backed
random access, partial reads, short-read diagnostics, the three identity
states, miss-to-hit behavior, invalidation on token change, corruption
recovery, `TilePlan` compatibility inputs, and deterministic diagnostics.

Tier 2 composes an external resolver, a local reproducible HTTP server, and a
COPC fixture through OpenStrata workspace composition, and verifies resolution,
metadata and range reads, local/remote output equivalence, reuse under stable
identity, and invalidation when validation metadata changes.

`usd-http-resolver` is the intended first external implementation. Its
2026-08-16 snapshot is documentation and an OpenStrata project skeleton only;
it has no resolver bundle, backend, cache, registered tests, or release. The
release sequence is consequently staged: finish and gate Tier 1 here first,
then run and record Tier 2 after the resolver publishes its first implementation
and OpenStrata workflow. The bundled test double remains test-only until that
evidence supports its removal or relocation.

#### Diagnostics

Cache decisions are explained through stable categories — identity
unavailable, unstable, stable, or changed; reuse disabled; cache hit; cache
invalidated — without leaking transport specifics or token contents. `Missing
HTTP ETag` is not an acceptable message because HTTP is not part of the
contract.

#### Non-goals

HTTP clients and range requests, `ETag` parsing, `If-Range` / `If-None-Match`,
redirects, retry, timeout, and TLS policy, proxies, cookies, authentication
protocols, signed URLs, S3 / Azure / GCS SDK integration, raw remote byte
caching, range-cache eviction, connection pooling, COPC writing, E57, public
custom point-cloud USD schemas, and renderer-controlled runtime streaming.

Exit gate: a documented identity contract for resolver-provided sources, reuse
enabled exactly where identity is sufficient, Tier 1 passing without an
external resolver, the bundled test resolver clearly marked test-only, and
recorded remote baselines including `bytes fetched / source size`. Before the
release is tagged, Tier 2 must be recorded against a released external resolver;
that evidence then decides whether the bundled test resolver is removed or
relocated.

### Research - Runtime Streaming

Runtime streaming asks whether a large COPC can be consumed without converting
all of it to USDC in advance:

```text
host LOD decision -> COPC node identity
                  -> RandomAccessSource::Read(offset, size)
                  -> decode -> render / authored representation
```

This depends on OpenUSD composition, payload loading, the host application, and
the renderer, so it is investigated as a separate theme with its own
experiments. Findings may inform the conversion pipeline; they do not get to
complicate it before the evidence exists. No runtime abstraction is added to
the FileFormat or converter architecture on speculation.

### Later - E57 and Other Point-Cloud Formats

E57 is valuable but is not the immediate next milestone. Its decoder should
enter the existing pipeline as an adapter:

```text
E57 decoder -> PointStream -> filters / tiling / authoring / cache
```

When a format is added, the reader takes on format concerns only. USD
authoring, tiling, caching, and OpenUSD dependencies stay out of it, and the
connection point is `PointStream` or `RandomAccessSource`.

If multi-scan E57 data requires a change below `PointStream`, that need is
evidence about the common contract, not permission to build a parallel
authoring path. Delimited text and other point-cloud formats are also deferred
until the infrastructure milestones above are mature.

## Position Toward v1.0

The release sequence completes the core vertical architecture:

```text
v0.5.0  Resolver-backed random access
v0.6.0  Source identity and generated cache
v0.7.0  Adaptive tiling
v0.8.0  Real-world I/O measurement
v0.9.0  TilePlan convergence
v0.10.0 External resolver identity and safe remote cache reuse
```

which corresponds to one path from source to authored output:

```text
Source / ArResolver -> RandomAccessSource / PointStream -> filtering / sampling
  -> sequential planner or native hierarchy -> TilePlan -> generated cache
  -> payload authoring -> FileFormat or Converter
```

After `v0.10.0` the project should consider a stabilization release or a move
toward `v1.0.0` rather than immediately expanding format count. E57 and other
new formats are not `v1.0` requirements. The stronger `v1.0` criterion is that
the format-independent ingestion, tiling, caching, resolver, and authoring
contracts are stable enough to support future formats without architectural
rewrites.

## Non-Goals

The following are explicitly not priorities, so that they do not reappear as
implicit work:

- increasing the number of supported formats as a goal in itself;
- embedding an HTTP client in `usdCopc`;
- owning network transport, authentication, or raw byte caching for any
  resolver-backed source;
- presenting a bundled test resolver as a production transport;
- making any resolver implementation a build-time dependency;
- giving FileFormat reads the full responsibility for production-scale
  conversion;
- implementing a renderer in this repository;
- putting host-specific LOD policy into core contracts;
- removing the spool before measuring it;
- ignoring a native hierarchy and treating every format as sequential;
- fixing a public custom USD schema before the metadata contract is stable.

## Public Contracts

The current `geo:*` metadata and point attributes are becoming a public data
contract. Their names, USD types, meaning, units, coordinate spaces, source
mappings, required status, and stability belong in a canonical point-cloud
metadata reference. A custom USD schema remains deferred until this contract
is stable across formats.

Tile-plan identity becomes a versioned contract in `v0.9.0`. Planner algorithm
changes affect cache compatibility, so the planner identity and version are
cache-key inputs alongside `maxPointsPerTile`, `minPointsPerTile`, `maxDepth`,
the sampling algorithm and version, the applied filters, and source identity.

Source identity and its stability classification become a public contract in
`v0.10.0`: what a resolver must supply, what each stability level permits, and
that validation tokens are opaque to every consumer. The binding statement is
the [resolver-backed source contract](../architecture/RESOLVER_SOURCE.md).

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
  interrupted-write behavior;
- equivalent authored output from a sequentially planned tile plan and a
  COPC-native tile plan describing the same partition.

From `v0.10.0`, resolver coverage is split in two tiers. Tier 1 runs
repository-local contract tests against fake or memory-backed resolver assets
and is the required CI gate; Tier 2 composes an external resolver
implementation and a local reproducible server for end-to-end verification
without making this repository structurally depend on it.

## Performance Baselines

Representative measurements record source size, point count, peak RSS, spool
size, output size, payload count, conversion time, range-read count, bytes
fetched, and cache hit ratio.

From `v0.8.0`, source bytes read, spool bytes written, spool bytes read,
payload bytes written, and effective I/O amplification are recorded alongside
them, because memory bounds alone do not describe the cost of a large
conversion. For remote COPC, `bytes fetched / source size` remains a primary
KPI: useful remote access must fetch only the required ranges.

## Guiding Question

> Does this make `usd-pointcloud-plugins` better OpenUSD point-cloud
> infrastructure, or does it merely add another importer?

Prefer the former. Releases v0.1 through v0.4 established format capability;
v0.5 through v0.10 prioritize infrastructure maturity before format expansion.
