# pointcloud-copc

`pointcloud-copc` is the thin OpenUSD FileFormat Plugin adapter for read-only
COPC assets. It delegates COPC metadata, hierarchy validation, and selected
LAZ chunk decoding to `usdCopc`, then uses the shared point-cloud authoring
path. The plugin opens the resolved asset through the active `ArResolver` and
adapts its `ArAsset` to the project-owned random-access source contract.

The repository's resolver-backed integration test uses the independent
`plugins/httpresolver` bundle. That bundle is a test double which serves a
local fixture as an in-memory asset; a production HTTP resolver can replace it
without changing this plugin or `usdCopc`.

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
not reused when that identity cannot be established. Remote tiled reads
require an absolute local `payloadDirectory`.
