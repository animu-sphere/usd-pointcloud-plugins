# usdGeoCache

`usdGeoCache` defines the OpenUSD-independent contract for deterministic
derived caches. A cache descriptor includes source identity, reader and
OpenUSD versions, coordinate settings, selected attributes, tile/LOD settings,
and downsampling settings. The descriptor is normalized through
`usdGeoCore::StableCacheKey` so equivalent argument order and whitespace do
not create different cache entries.

Each descriptor maps to one entry directory with reserved paths for
`root.usdc`, a `cache.manifest`, and a `payloads/` directory. `TilePayloadPath`
uses the same stable tile naming shape as payload-backed authoring. Cache
entries are deletable derived data. The manifest is the generation commit
marker: `Inspect` reports `Hit` only when both `root.usdc` and
`cache.manifest` are regular files, so a root layer left behind by an
interrupted generation is reported as `Incomplete` and is not reusable.
`Inspect` also reports `Missing` when neither file exists and `InvalidLayout`
for an invalid layout. `IsCacheHit` remains the compatibility boolean wrapper
around this status contract. `Invalidate` recomputes the descriptor entry
below the supplied cache root and never accepts an arbitrary layout path, so it
cannot delete an unrelated sibling directory.
`LookupStatusName` exposes stable machine-readable status names, and the
cache-library `GetLookupStatistics` snapshot counts every `Inspect` call by
status. Updates, snapshots, and resets are serialized so the counters remain
internally consistent during concurrent lookups. `ResetLookupStatistics`
clears the counters for a new measurement; `LookupStatistics::HitRatio` returns
zero when no lookup has been recorded. The conversion tool prints the
statistics for cache-enabled runs; separate statically linked plugin modules
maintain separate counters.
The shared authoring cache loader also opens the committed root and validates
every payload reference before reuse. A root that cannot be opened, a missing
payload, or a payload outside the entry is treated as a corrupt entry and
invalidated; failures materializing valid cached payloads into a caller-owned
directory do not invalidate the cache entry.

## Compatibility and Invalidation

An entry is compatible only when its descriptor produces the same stable key.
The key includes source identity and validation data, reader and OpenUSD
versions, normalized coordinate and attribute settings, tile/LOD settings, and
downsampling settings. Changing any of those values selects a different entry;
the cache never treats an older entry as compatible by inspecting only the
source path.

Cache entries are derived data and may be deleted at any time. A missing entry
is a cache miss. An entry with only one commit marker, an unreadable root, a
missing payload, or a payload reference outside the entry is incomplete or
corrupt and must not be reused. The shared authoring loader invalidates corrupt
entries through the descriptor-derived path. Interrupted conversion output is
also not committed until both the root and manifest are published.

CMake target `usdgeo::cache` and namespace `usdgeo::cache`.

The module owns identity, layout, lookup, and invalidation only. It does not
open or author USD, or decide reader-specific cache arguments. `SourceIdentity`
accepts a neutral identifier and validation token, so local paths, resolver
identifiers, ETags, and digests can participate without transport-specific
types entering the cache contract. `TryBuildLocalSourceIdentity` is the
filesystem convenience helper for local callers; it supplies a canonical
identifier and an `fnv1a64` validation token. The legacy `canonicalPath` and
`contentIdentity` fields remain accepted as compatibility aliases.