# pointcloud-copc

`pointcloud-copc` is the thin OpenUSD FileFormat Plugin adapter for local,
read-only COPC files. It delegates COPC metadata, hierarchy validation, and
selected LAZ chunk decoding to `usdCopc`, then uses the shared point-cloud
authoring path.

The initial adapter supports metadata-only and non-tiled reads. Source point
ranges are rejected because COPC hierarchy order is spatial rather than LAS
source order; bounds and classification filters are applied while decoded
points pass through a bounded buffer. COPC writing, network range sources,
tiled `PointStream` reads, and payload generation remain outside this first
plugin slice.
