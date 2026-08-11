# Stanford Bunny subset

- Source dataset: Stanford 3D Scanning Repository, reconstructed Bunny
- Source URL: https://graphics.stanford.edu/pub/3Dscanrep/bunny.tar.gz
- Source member: `bunny/reconstruction/bun_zipper.ply`
- Source SHA-256: `b1acc63bece78444aa2e15bdcc72371a201279b98c6f5d4b74c993d02f0566fe`
- Source properties: 35,947 ASCII vertices, plus mesh faces
- Terms: the Stanford repository permits research use and free redistribution
  with credit to the Stanford Computer Graphics Laboratory; commercial use is
  not permitted without permission. See the repository's “Please acknowledge”
  and “Inappropriate uses” sections.
- Attribution: Stanford Computer Graphics Laboratory; Stanford 3D Scanning
  Repository
- Derivation: discard mesh faces and select exactly 4,096 vertices at
  deterministic, evenly spaced indices from source order, including the first
  and last vertices. Preserve all scalar vertex values and rename the source
  float `intensity` property to generic `scanner_intensity`; the reader's
  meaning-specific `intensity` property is reserved for uint16 values. Write
  an ASCII point-only PLY.
- Output: `stanford-bunny-thinned-4096.ply`, 4,096 points
- Output SHA-256: `1834e38722dcfd8c74a2a913aabea86940c7cf07ffc38425c4492ad94d5d8526`
- Regeneration:
  `python tools/thin_ply_corpus.py <bunny>/reconstruction/bun_zipper.ply
  plugins/pointcloud-ply/tests/corpus/stanford-bunny/
  stanford-bunny-thinned-4096.ply --target-count 4096
  --rename-property intensity=scanner_intensity`