# File Format Support Order

## Selection Criteria

Formats are prioritized by their ability to validate the shared architecture, deliver practical value, and reuse earlier work. A format moves into active implementation only when its required foundation and test data are available.

Priority is determined by:

1. Value for surveying, point-cloud, terrain, and geospatial workflows
2. Ability to validate direct OpenUSD FileFormat Plugin access
3. Reuse of existing readers, metadata contracts, tiling, and cache infrastructure
4. Availability and licensing of portable dependencies
5. Partial-read and spatial-index capabilities
6. Complexity of preserving source metadata without loss

## Support Sequence

| Order | Format | Initial scope | Why this position | Entry gate |
| --- | --- | --- | --- | --- |
| 1 | LAS | LAS 1.2-1.4 headers, common point formats, VLR/EVLR, XYZ, intensity, returns, classification, RGB, GPS time, CRS | Uncompressed records expose the complete coordinate and point-attribute model without codec complexity | `usdGeoCore`, `usdPointCloudCore`, and a minimal `usdGeoUsd` writer are tested |
| 2 | LAZ | The LAS logical model with chunked decompression and the same file-format arguments | Delivers the common production format while reusing LAS semantics | LAS conformance corpus passes and the codec decision is recorded |
| 3 | COPC | Read existing hierarchy, bounds, chunks, and LOD metadata; no writer initially | Validates native spatial hierarchy and partial loading before inventing a repository-specific point-cloud container | Stable LAZ chunk reading and tile contracts exist |
| 4 | GeoTIFF | Elevation bands, CRS, geotransform, NoData, bounds, and mesh or heightfield output | First non-point-cloud format validates that `usdGeoCore` is genuinely shared | CRS and local-origin contracts work across independently authored datasets |
| 5 | COG | GeoTIFF semantics plus overview and range-based tile access | Reuses GeoTIFF interpretation and validates remote/partial raster loading | GeoTIFF correctness is stable and tile-provider APIs exist |
| 6 | GeoJSON | Point, LineString, Polygon, Multi* geometries, feature properties, CRS policy, deterministic prim naming | Small fixtures and simple parsing make it the best first vector format | `usdVectorCore` geometry and property contracts are tested |
| 7 | FlatGeobuf | Indexed feature reads and large vector datasets | Extends the GeoJSON model with spatial indexing and partial reads | GeoJSON mapping and vector tiling contracts are stable |
| 8 | E57 | Multiple scans, sensor poses, timestamps, common point attributes, optional images later | High-value but semantically different from LAS; requires a dedicated reader and scan model | Point-cloud contracts support multiple scans without LAS-specific assumptions |
| 9 | GeoPackage | Read-only, selected feature tables and metadata | Useful interoperability format, but database and layer-selection semantics broaden the dependency surface | Vector layer selection and SQLite dependency policy are settled |
| 10 | Shapefile | Read-only compatibility for common geometry and DBF attributes | Legacy value does not justify preceding indexed or self-describing vector formats | Encoding, sidecar, and missing-file policies are defined |

## Delivery Stages Per Format

Every format progresses through the same stages:

1. **Inspect**: parse metadata and report unsupported features without authoring a stage.
2. **Decode**: produce validated project-owned intermediate data.
3. **Direct read**: open the source as an OpenUSD layer.
4. **Selective read**: support normalized attributes, bounds, LOD, or layer arguments where applicable.
5. **Cache**: generate and reuse deterministic USDC cache content.
6. **Scale**: add chunking, tiling, parallelism, and recovery for large or damaged data.

A later stage must not delay a correct implementation of an earlier stage. Write-back support is a separate future decision and is not part of initial format completion.

## Point-Cloud Milestones

### Milestone A: LAS Preview

- Open `.las` directly as an OpenUSD layer.
- Author XYZ and available RGB or intensity as `UsdGeomPoints`.
- Preserve CRS, Scale, Offset, bounds, and source point-format metadata.
- Reconstruct source coordinates within the documented error budget.

### Milestone B: LAZ Production Read

- Open `.laz` through the same logical model and argument contract as LAS.
- Decode incrementally rather than materializing the compressed source at once.
- Select attributes and enforce deterministic point limits.
- Reuse USDC cache content across equivalent normalized arguments.

### Milestone C: Tiled Point Clouds

- Represent source or generated spatial hierarchy through shared tile contracts.
- Read only required COPC or generated tiles.
- Provide two or three deterministic nested LOD levels.
- Connect tile representations to the verified OpenUSD 26.08 LOD mechanism.

## Terrain Milestone

GeoTIFF and COG are complete for the first terrain milestone when elevation data and LAS / LAZ point clouds can be placed in one stage through the same CRS, unit, and local-origin contracts. Differences in horizontal CRS, vertical reference, units, and NoData handling must be visible diagnostics.

## Vector Milestone

GeoJSON and FlatGeobuf are complete for the first vector milestone when equivalent features produce the same project-owned geometry and property model, deterministic prim paths, and compatible spatial bounds. Large FlatGeobuf datasets must support bounded reads without decoding the complete file.

## Deferred Formats and Features

The following remain deferred until a concrete workflow and ownership boundary are established:

- Full LAS / LAZ write-back and round-trip guarantees
- Waveform packet interpretation
- E57 image extraction and advanced scan reconstruction
- Arbitrary raster imagery and multidimensional GDAL datasets
- Full GeoPackage raster support
- 3D Tiles terrain and tileset ingestion
- Proprietary vendor VLR interpretation
- Renderer-specific point primitives or shaders
