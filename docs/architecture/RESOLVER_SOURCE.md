# Resolver-backed source contract

This document fixes the boundary between `usd-pointcloud-plugins` and whatever
resolves and transports a source asset. It covers resolver-backed byte access,
resolver-neutral source identity, generated-cache ownership, and the
diagnostics that explain a cache decision.

Structure belongs to [WORKSPACE.md](WORKSPACE.md); this document owns the
resolver-facing behavior those modules implement. Cache layout and
invalidation belong to the [`usdGeoCache` README](../../libs/usd-geo-cache/README.md).

Everything below is shipped as of `v0.10.0` unless a section says otherwise.
What the tree implements is in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md); the recorded Tier 2
numbers are in [RESOLVER_BASELINE.md](../reference/RESOLVER_BASELINE.md).

## 1. Responsibility boundary

The project consumes resolver-provided assets through OpenUSD abstractions. It
does not implement transport:

```text
resolver implementation            (separate repository)
  ├─ HTTP / HTTPS, S3, or other transport
  ├─ range requests and redirects
  ├─ retry, timeout, and TLS policy
  ├─ authentication and credentials
  ├─ transport validation metadata
  ├─ raw byte / range cache
  └─ ArResolver / ArAsset implementation
                 |
                 | OpenUSD resolver contract
                 v
usd-pointcloud-plugins
  ├─ resolver-backed asset consumption
  ├─ RandomAccessSource adaptation
  ├─ resolver-neutral SourceIdentity
  ├─ COPC metadata, hierarchy, and range decoding
  ├─ TilePlan
  ├─ generated USD cache
  ├─ payload authoring
  └─ usd-pointcloud-convert
```

Invariants:

- No module in this repository implements an HTTP client, a cloud SDK, an
  authentication flow, a retry policy, or a raw byte-range cache.
- No build-time dependency on any resolver implementation exists: no CMake
  dependency, submodule, vendored transport library, resolver-specific include,
  or link dependency. Resolvers are runtime composition.
- `usdCopc` stays independent of OpenUSD and of transport. It sees only
  `RandomAccessSource`.
- The point-cloud plugin never learns which resolver opened an asset.

## 2. Source identity

Source identity describes **what source content was resolved**, not **how it
was transported**. Transport-specific field names such as `url` or `etag` do
not enter the cache contract.

`usdgeo::cache::SourceIdentity` carries a neutral triple:

```text
resolvedIdentifier   canonical or resolved source identifier
size                 source byte size when known
validationToken      opaque value that changes when source content changes
```

The point-cloud code treats `validationToken` as opaque. A resolver may derive
it from an HTTP `ETag`, `Last-Modified` plus size, an S3 object version id, a
cloud-object generation number, local filesystem metadata, a content digest, or
a studio asset revision. None of those concepts appear here.

Local sources build this identity through `TryBuildLocalSourceIdentity`, which
supplies a canonical path and an `fnv1a64` validation token.

### 2.1 Stability levels

Resolver-provided identity is classified before it is trusted:

```text
Stable        sufficient for safe generated-cache reuse
Unstable      readable source, but freshness cannot be guaranteed
Unavailable   the resolver exposes no usable identity metadata
```

Generated-cache reuse stays disabled for `Unstable` and `Unavailable`. That is
the fail-safe behavior established before `v0.10.0`, and enabling reuse for
`Stable` identity is the only relaxation the milestone makes.

### 2.2 Adapter placement

One resolver-facing adapter translates resolver-provided asset identity into
the cache identity model:

```text
ArResolver
    |
    v
resolved asset --> resolved identifier, size, validation metadata
    |
    v
ResolverAssetIdentity
    |
    v
SourceIdentity
```

The adapter lives outside format-specific readers. LAS, LAZ, COPC, and PLY code
must not each implement identity extraction. Ownership is `usdGeoCache` or a
small resolver integration layer adjacent to it, so a future raster or vector
repository can reuse the same abstraction. Point-cloud tiling concepts do not
move into `usdGeoCore` for this.

### 2.3 Secrets

Resolver credentials, authorization headers, signed URLs, and access tokens are
never serialized into generated manifests, cache descriptors, or diagnostics.

## 3. Cache ownership

Two caches exist, with one owner each:

```text
raw byte / range cache        -> resolver implementation
generated USDC / payload cache -> usd-pointcloud-plugins
```

A resolver-backed generated-cache key uses the same deterministic generation
inputs as a local source, plus stable source identity:

```text
GeneratedCacheKey =
    SourceIdentity
  + format arguments
  + selected attributes
  + filters
  + tiling arguments
  + TilePlan identity and version
  + authoring compatibility version
```

`TilePlan` planner identity and version, introduced in `v0.9.0`, remain
compatibility inputs.

### 3.1 Reuse rules — Implemented (`v0.10.0`)

A resolver-backed entry may be reused only when all of the following hold:

- source identity is `Stable`;
- the identity matches the cached descriptor;
- generation arguments match;
- `TilePlan` compatibility inputs match;
- the committed root and every referenced payload validate.

Any other outcome fails closed and regenerates. Equal identifiers never imply
equal content: a URL match alone is not a cache hit.

The central correctness property is revision change:

```text
source revision A -> miss -> generate -> hit
source revision B, same identifier, different validation token
                  -> old entry MUST NOT hit -> regenerate
```

### 3.2 FileFormat and converter behavior

Resolver-backed FileFormat reads stay preview and inspection paths. They reuse
generated cache only under stable identity, and they diagnose explicitly when
reuse is disabled rather than silently regenerating. This is shipped: the COPC
FileFormat consumes a committed entry under `Stable` identity and reports every
other outcome through the categories in §4.

`usd-pointcloud-convert` remains the production path for deterministic,
long-running payload generation, and it is the only thing that publishes a
generated entry. **Not implemented (`v0.10.0`):** it accepts `.las` and `.laz`
local inputs only. No COPC input, and no resolver-addressable identifier,
reaches it, so no COPC read — local or resolver-backed — has an entry to hit in
a normal workflow. The lookup side is complete and covered; the generation side
for COPC and for resolver-addressable inputs is future work. When it lands it
computes the same resolver-neutral identity, populates and reuses the generated
cache only under stable identity, and may record the normalized identity class
in manifest or debug metadata — never the transport secrets covered in §2.3.

### 3.3 Cache ownership boundary — Implemented (`v0.10.0`)

The generated-USDC cache is owned by `usdGeoCache` and
`usd-pointcloud-authoring`. Its entries contain the committed `root.usdc`,
manifest, and generated payloads, and their identity includes the resolver
validation token.

An entry's path is two levels, and both components are 64-bit hashes:

```text
<cache root>/<generation key>/<source identity key>/
```

The split follows one rule: caller intent chooses the directory, and everything
read out of the source chooses the entry inside it. The generation key covers
the resolved identifier, the plugin, parser, and OpenUSD versions, attribute
selection, tiling and LOD arguments including planner identity and version, and
downsampling. The source identity key covers the revision metadata - size,
modification time, and the opaque validation token - plus the georeference
resolved from the source header and any plan computed by scanning it.

The georeference belongs in the second half because it is source-derived: the
local origin is the source bounding box, and the CRS may be an embedded record.
Putting it in the first half would mean a revision whose bounding box moved
landed in an unrelated generation directory, where nothing could see that it
superseded anything. The caller's *explicit* coordinate arguments are a
different value and stay in the generation key with the rest of the normalized
arguments.

Two consequences follow, and both are load-bearing. Revisions of one source
collect side by side under one generation directory, which is how a changed
validation token is reported as `resolver-identity-changed` rather than as a
source never seen before. And neither level renders an identifier or a token, so
a signed URL cannot be read back out of a cache root — see §2.3.

Source byte-range caching is a separate concern owned by the active resolver
and its `ArAsset` implementation. The point-cloud readers and
`ArAssetRandomAccessSource` perform bounded reads but do not persist source
ranges in the generated-USDC cache. This prevents transport or resolver
fetch state from becoming an implicit generated-asset cache key or artifact.

## 4. Diagnostics — Implemented (`v0.10.0`)

Cache decisions are explained through stable categories. `usdgeo::cache`
publishes them as `CacheDecision`, and `CacheDecisionName` is the string form a
consumer matches on:

| Category | Name |
| --- | --- |
| resolver identity unavailable | `resolver-identity-unavailable` |
| resolver identity unstable | `resolver-identity-unstable` |
| resolver identity stable | `resolver-identity-stable` |
| resolver identity changed | `resolver-identity-changed` |
| generated cache reuse disabled | `generated-cache-reuse-disabled` |
| generated cache hit | `generated-cache-hit` |
| generated cache invalidated | `generated-cache-invalidated` |

Names are stable once published, in the sense rule 1 of the
[diagnostics contract](DIAGNOSTICS.md) defines: a name is never reused for a
different meaning. Messages are for humans and may change between releases.

Messages describe the decision without leaking transport specifics or token
contents:

```text
Generated cache reuse disabled: the active resolver did not provide
a stable source validation identity.
```

`Missing HTTP ETag` is not an acceptable message, because HTTP is not part of
this contract. Every message is a fixed constant owned by `usdgeo::cache`, so a
transport detail cannot reach one by accident, and a unit test asserts that none
of them names one.

Codes project onto the existing prefixes described in the
[diagnostics contract](DIAGNOSTICS.md). The COPC projection is four codes over
the seven categories, and every emitted message names its exact category:

| Code | Severity | Categories |
| --- | --- | --- |
| `COPC009` | warning | identity unavailable, identity unstable, reuse disabled |
| `COPC010` | status | identity stable, cache hit |
| `COPC011` | status | identity changed |
| `COPC012` | warning | cache invalidated |

`resolver-identity-changed` is observable because the generated-cache layout
separates what would be generated from which revision was read; see §3.3.

## 5. Interoperability

An external resolver and these plugins compose at runtime through
`PXR_PLUGINPATH_NAME`:

```text
usdview https://example.org/data.copc
       |
       v
OpenUSD ArResolver
       |
       v
external resolver implementation
       |
       v
ArAsset
       |
       v
pointcloud-copc
```

`usd-http-resolver` is one compatible implementation, not a required
dependency. Registration is in [INSTALL.md](../guides/INSTALL.md).

## 6. Testing tiers — Implemented (`v0.10.0`)

**Tier 1 — repository-local contract tests.** They run with no external
resolver repository, using fake or memory-backed test assets, and are the
required gate on every host and both lanes. `openstrata.ci.yaml` declares
`kind: workspace` cells that configure the repository root — where
`USDGEO_BUILD_TESTS` defaults to `ON` — and run its CTest suite; a per-plugin
bundle cell cannot compile these tests. Coverage: resolver-backed random access,
partial reads, short-read diagnostics, stable / unstable / unavailable identity,
miss-to-hit behavior, invalidation on validation-token change, superseded-entry
detection, corruption recovery, `TilePlan` compatibility in cache keys, and
deterministic diagnostics.

**Tier 2 — cross-repository integration — recorded (`v0.10.0`).** An external resolver, a local
reproducible HTTP server, and a COPC fixture verify that a URL resolves, that
metadata, hierarchy, and point-range reads succeed, that authored local and
resolver-backed output is equivalent, that stable identity enables reuse, and
that changed validation metadata invalidates it. Raw range caching stays owned
and tested by the resolver repository. Tier 2 is reproducible through
OpenStrata workspace composition without making this repository structurally
dependent on the resolver repository.

[`usd-http-resolver`](https://github.com/animu-sphere/usd-http-resolver) is the
first Tier 2 implementation, and `v0.10.0` is recorded against its `v0.4.0`
release, which provides the HTTP backend, the OpenUSD resolver bundle, and
resolver-neutral identity through `ArAssetInfo`.
`tools/tier2_fixture_server.py` is the loopback origin and
`tools/tier2_resolver_integration.py` the harness; the numbers are in
[RESOLVER_BASELINE.md](../reference/RESOLVER_BASELINE.md).

The recorded run shows a metadata open costing 0.15% of an 81 MB asset, a full
read costing exactly 1.0, local and resolver-backed reads authoring the same
10,653,336 points under the same digest, a strong validator classifying as
`Stable`, and a weak validator classifying as `Unstable` and disabling reuse
while authoring identical output.

## 7. Test-double resolver

`tests/plugins/httpresolver` is an integration-test fixture: it serves a configured
local fixture as an in-memory `ArAsset` for `http://memory.copc` and
`https://memory.copc`. It is not a network transport and is not part of the
point-cloud product surface.

The test double remains as the external-dependency-free Tier 1 fixture. It is
built only with the COPC integration tests and has no product bundle manifest
or install rule, so it is excluded from plugin discovery, release metadata,
packaging, and standalone CI cells. It must not be presented as equivalent to
a production resolver bundle. See
[WORKSPACE.md](WORKSPACE.md).
