# OpenUSD Compatibility

This document states which runtimes the plugins are built and tested against,
and what they require from a host application.

## Declared Contract

Both plugin manifests declare:

```yaml
runtime:
  openusd: ">=26.08,<27.0"
```

`geo-las` and `geo-laz` are `usd-fileformat` bundles and require the
`usd-stage-read` capability.

| Item | Value |
| --- | --- |
| Validated OpenUSD version | 26.08 |
| Accepted range | `>=26.08,<27.0` |
| OpenStrata CLI | 0.21.0 |
| OpenStrata platform / profile | `cy2026` / `usd` |
| C++ standard | C++17 |
| CMake | 3.23 or newer |

A 27.x runtime is outside the declared range. Raising the upper bound requires
rebuilding and re-running the plugin integration tests against that runtime.

## Tested Platforms

| Platform | Runner | Coverage |
| --- | --- | --- |
| Windows x86_64 | `windows-2022` | Plugin build and integration tests |
| Linux x86_64 | `ubuntu-24.04` | Plugin build and integration tests |
| macOS arm64 | `macos-15` | Plugin build and integration tests |

The core libraries (`usdGeoCore`, `usdPointCloudCore`, `usdLas`, `usdLaz`)
build and test with plain CMake and no OpenUSD runtime. Only `usdGeoUsd` and
the plugin bundles require OpenUSD.

## OpenUSD Surface Used

The plugins depend on a small, stable part of the API:

- `SdfFileFormat`, `SdfLayer`, and `SdfFileFormat::FindByExtension("usda")`
- `UsdStage::Open` on an anonymous layer, then `SdfLayer::TransferContent`
- `UsdGeomPoints`, `UsdGeomSetStageUpAxis`, `UsdGeomSetStageMetersPerUnit`
- `TfType` registration through `TF_REGISTRY_FUNCTION` and
  `SDF_DEFINE_FILE_FORMAT`
- `VtArray`, `GfVec3f`, and `GfVec3d` for authored values

No API schema, no Hydra scene index, and no LOD or payload mechanism is used
yet. Tile and LOD work will bind to OpenUSD 26.08 mechanisms inside
`usdGeoUsd` only; see the [development policy](../development-policy.md).

## Host Expectations

- The host discovers plugins through `PXR_PLUGINPATH_NAME`; see the README.
- `Read` requires a writable layer and full point data. `metadataOnly` reads
  are refused.
- Authored stages use up axis `Y` and `metersPerUnit` 1.0 regardless of the
  source CRS units. The source-to-stage relationship is recorded in the
  `geo:*` attributes documented in
  [supported formats](../supported-formats.md).
