# pointcloud-copc

`pointcloud-copc` is the thin OpenUSD FileFormat Plugin adapter for local,
read-only COPC files. It delegates COPC metadata, hierarchy validation, and
selected LAZ chunk decoding to `usdCopc`, then uses the shared point-cloud
authoring path.

The initial adapter supports metadata-only and non-tiled reads. COPC writing,
network range sources, tiled `PointStream` reads, and payload generation remain
outside this first plugin slice.
