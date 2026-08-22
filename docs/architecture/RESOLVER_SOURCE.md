# Resolver-backed source contract

This document fixes the boundary between `usd-pointcloud-plugins` and whatever
resolves and transports a source asset. It covers resolver-backed byte access,
resolver-neutral source identity, generated-cache ownership, and the
diagnostics that explain a cache decision.

Structure belongs to [WORKSPACE.md](WORKSPACE.md); this document owns the
resolver-facing behavior those modules implement. Cache layout and
invalidation belong to the [`usdGeoCache` README](../../libs/usd-geo-cache/README.md).

Sections marked **Planned (`v0.10.0`)** are direction, not shipped behavior.
What the tree implements today is in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md).

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
reuse is disabled rather than silently regenerating.

`usd-pointcloud-convert` remains the production path for deterministic,
long-running payload generation. For resolver-backed inputs it accepts
resolver-addressable identifiers where the active OpenUSD environment supports
them, computes the same resolver-neutral identity, populates and reuses the
generated cache only under stable identity, and may record the normalized
identity class in manifest or debug metadata — never the transport secrets
covered in §2.3.

### 3.3 Cache ownership boundary — Implemented (`v0.10.0`)

The generated-USDC cache is owned by `usdGeoCache` and
`usd-pointcloud-authoring`. Its entries contain the committed `root.usdc`,
manifest, and generated payloads, and their identity includes the resolver
validation token.

Source byte-range caching is a separate concern owned by the active resolver
and its `ArAsset` implementation. The point-cloud readers and
`ArAssetRandomAccessSource` perform bounded reads but do not persist source
ranges in the generated-USDC cache. This prevents transport or resolver
fetch state from becoming an implicit generated-asset cache key or artifact.

## 4. Diagnostics — Planned (`v0.10.0`)

Cache decisions are explained through stable categories:

```text
resolver identity unavailable
resolver identity unstable
resolver identity stable
resolver identity changed
generated cache reuse disabled
generated cache hit
generated cache invalidated
```

Messages describe the decision without leaking transport specifics or token
contents:

```text
Generated cache reuse disabled: the active resolver did not provide
a stable source validation identity.
```

`Missing HTTP ETag` is not an acceptable message, because HTTP is not part of
this contract. Codes project onto the existing prefixes described in the
[diagnostics contract](DIAGNOSTICS.md).

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

## 6. Testing tiers — In progress (`v0.10.0`)

**Tier 1 — repository-local contract tests.** They run with no external
resolver repository, using fake or memory-backed test assets, and remain the
required CI gate. Coverage: resolver-backed random access, partial reads,
short-read diagnostics, stable / unstable / unavailable identity, miss-to-hit
behavior, invalidation on validation-token change, corruption recovery,
`TilePlan` compatibility in cache keys, and deterministic diagnostics.

**Tier 2 — cross-repository integration.** An external resolver, a local
reproducible HTTP server, and a COPC fixture verify that a URL resolves, that
metadata, hierarchy, and point-range reads succeed, that authored local and
resolver-backed output is equivalent, that stable identity enables reuse, and
that changed validation metadata invalidates it. Raw range caching stays owned
and tested by the resolver repository. Tier 2 is reproducible through
OpenStrata workspace composition without making this repository structurally
dependent on the resolver repository.

[`usd-http-resolver`](https://github.com/animu-sphere/usd-http-resolver) is the
designated first Tier 2 implementation. Its `v0.2.0` release provides the HTTP
backend and OpenUSD resolver bundle, exposes stable resolver-neutral identity
through `ArAssetInfo`, and is tested through its own OpenStrata workflow. Tier
1 remains this repository's required CI gate; Tier 2 is now ready to be
composed and recorded as the `v0.10.0` release gate.

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
