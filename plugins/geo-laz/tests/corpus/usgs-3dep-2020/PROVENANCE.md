# USGS 3DEP 2020 subset

- Source dataset: USGS 3D Elevation Program, `CO_DRCOG_2020_B20`
- Source tile: `w0474n4422`
- Source URL: https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/LPC/Projects/CO_DRCOG_2020_B20/CO_DRCOG_1_2020/LAZ/USGS_LPC_CO_DRCOG_2020_B20_w0474n4422.laz
- Catalog metadata: https://tnmaccess.nationalmap.gov/api/v1/products?datasets=Lidar%20Point%20Cloud%20(LPC)&bbox=-105.30,39.95,-105.29,39.96&max=5
- License: USGS public domain data
- Source properties: 30,593,822 points, LAS 1.4 point format 6, scale 0.001 m
- Derivation: select exactly 4096 points at deterministic, evenly spaced indices
  from the source point order, including the first and last points. Preserve
  the selected coordinates and supported attributes, then normalize the output
  to LAS 1.2 point format 3 for compatibility with the current LAZ decoder. The
  sibling LAS asset uses the same selected points.
- Output: `usgs-3dep-2020-thinned-4096.laz`, 4096 points, LAS 1.2 point format 3
- Output SHA-256: `dd5a5b6283cd44ac9077c1403f3d83fd4c4753851b9742b77b6259eae6b3fb`
- Regeneration: read the source LAZ with `laspy` in 1,000,000-point chunks,
  select `numpy.linspace(0, point_count - 1, 4096, dtype=numpy.int64)`, copy
  the supported fields into a LAS 1.2 point format 3 header, and write LAZ.