# Virtual Shizuoka 08NF2330 subset

- Source dataset: Virtual Shizuoka, Shizuoka Prefecture, 2019 LP ground data
- Source tile: `08NF2330.las`
- Source archive: https://virtual-shizuoka.s3.ap-northeast-1.amazonaws.com/2019/LP/Ground/08/NF/23/08NF2330.zip
- Dataset metadata: https://www.geospatial.jp/ckan/dataset/shizuoka-2019-pointcloud
- License: CC BY 4.0/ODbL dual license, as stated by the dataset metadata
- Attribution: Virtual Shizuoka / Shizuoka Prefecture
- Source properties: 14,574,030 points, LAS 1.2 point format 3, scale 0.001 m
- Derivation: select exactly 4096 points at deterministic, evenly spaced indices
  from the source point order, including the first and last points. Preserve
  the source LAS header and point attributes. The sibling LAZ asset uses the
  same selected points.
- Output: `virtual-shizuoka-08NF2330-thinned-4096.las`, 4096 points
- Output SHA-256: `a2ecbc84fb21af175e4f29a43f6894b3399ff1ab1f4d142f2224e2cee6b642ad`
- Regeneration: read `08NF2330.las` with `laspy`, select
  `numpy.linspace(0, point_count - 1, 4096, dtype=numpy.int64)`, and write
  the selected points as LAS.