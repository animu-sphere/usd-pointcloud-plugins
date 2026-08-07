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
marker: `IsCacheHit` requires both `root.usdc` and `cache.manifest`, so a root
layer left behind by an interrupted generation is not reusable. `Invalidate`
recomputes the descriptor entry below the supplied cache root and never accepts
an arbitrary layout path, so it cannot delete an unrelated sibling directory.

CMake target `usdgeo::cache` and namespace `usdgeo::cache`.

The module owns identity, layout, lookup, and invalidation only. It does not
open or author USD, calculate source hashes, or decide reader-specific cache
arguments. Callers must supply a canonical source path and content identity.