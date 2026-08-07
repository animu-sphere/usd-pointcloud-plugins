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
entries are deletable derived data: `Invalidate` removes the complete
descriptor directory and never touches the source file.

CMake target `usdgeo::cache` and namespace `usdgeo::cache`.

The module owns identity, layout, lookup, and invalidation only. It does not
open or author USD, calculate source hashes, or decide reader-specific cache
arguments. Callers must supply a canonical source path and content identity.