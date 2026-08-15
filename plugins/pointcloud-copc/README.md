# pointcloud-copc

`pointcloud-copc` is the thin OpenUSD FileFormat Plugin adapter for read-only
COPC assets. It delegates COPC metadata, hierarchy validation, and selected
LAZ chunk decoding to `usdCopc`, then uses the shared point-cloud authoring
path. The plugin opens the resolved asset through the active `ArResolver` and
adapts its `ArAsset` to the project-owned random-access source contract.

Transport is not this plugin's responsibility. Whatever resolver the host
configures owns the network protocol, authentication, retries, redirects, and
raw byte caching, and the plugin never detects which resolver opened the asset.
An external resolver such as `usd-http-resolver` is runtime composition through
`PXR_PLUGINPATH_NAME`, not a build-time dependency of this bundle.

The repository's resolver-backed integration test uses the independent
`plugins/httpresolver` bundle. That bundle is a test double which serves a
local fixture as an in-memory asset; a production resolver can replace it
without changing this plugin or `usdCopc`. See the
[resolver-backed source contract](../../docs/architecture/RESOLVER_SOURCE.md).

The adapter supports metadata-only reads, non-tiled reads, and native
hierarchy tiles authored as payload-backed `usdLod` roots. Each COPC
point-data node becomes one shared tile with its native spacing preserved.
Source point ranges are rejected because COPC hierarchy order is spatial
rather than LAS source order; bounds and classification filters are applied
while decoded points pass through a bounded buffer. Remote COPC is supported
only when the active resolver supplies an asset with efficient random-access
reads, such as HTTP byte-range support. The plugin does not implement an HTTP
client, transport retries, or a network cache. Generated-USDC cache lookup is
limited to stable local filesystem identities; resolver-backed sources are
not reused when that identity cannot be established. The integration baseline
keeps this behavior covered with `USDGEO_CACHE_ROOT` configured. Remote tiled
reads require an absolute local `payloadDirectory`.
