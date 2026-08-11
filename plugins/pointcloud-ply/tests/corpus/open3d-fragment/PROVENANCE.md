# Open3D fragment subset

- Source project: Open3D test data
- Source URL: https://github.com/isl-org/Open3D/blob/b58440d61b882a5d0849a329c08a6bd8b85e15ba/examples/test_data/fragment.ply
- Source revision: `b58440d61b882a5d0849a329c08a6bd8b85e15ba`
- Source SHA-256: `96203542a5659c1efd2bcfe8a2dd7d3ffc9b88cf24680b4703515f6b21f67d99`
- Source properties: 196,133 binary little-endian vertices with RGB, normals,
  and curvature; one camera element is omitted from the point subset
- Terms: the source file was distributed as Open3D test data. The source file
  has no separate license notice in the pinned tree; this derived asset is
  retained for repository test use only and is not installed or packaged.
  Confirm upstream dataset terms before any public binary or corpus release.
- Attribution: Open3D contributors; source revision above
- Derivation: discard the camera element and select exactly 8,192 vertices at
  deterministic, evenly spaced indices from source order, including the first
  and last vertices. Preserve all scalar vertex properties and write an ASCII
  point-only PLY.
- Output: `open3d-fragment-thinned-8192.ply`, 8,192 points
- Output SHA-256: `e7a87f85eb0e6ee5846b4ada64c6b196f92bb29492720c9fbd79ad88733f8a1a`
- Regeneration:
  `python tools/thin_ply_corpus.py <Open3D>/examples/test_data/fragment.ply
  plugins/pointcloud-ply/tests/corpus/open3d-fragment/
  open3d-fragment-thinned-8192.ply --target-count 8192`