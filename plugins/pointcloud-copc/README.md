# pointcloud-copc

`pointcloud-copc` is the thin OpenUSD FileFormat Plugin adapter for local,
read-only COPC files. It delegates COPC metadata, hierarchy validation, and
selected LAZ chunk decoding to `usdCopc`, then uses the shared point-cloud
authoring path.

The adapter supports metadata-only reads, non-tiled reads, and native
hierarchy tiles authored as payload-backed `usdLod` roots. Each COPC
point-data node becomes one shared tile with its native spacing preserved.
Source point ranges are rejected because COPC hierarchy order is spatial
rather than LAS source order; bounds and classification filters are applied
while decoded points pass through a bounded buffer. COPC writing and network
range sources remain outside the local read path.
