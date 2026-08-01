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
yet.

## Planned LOD Surface

Tile and LOD work binds to the OpenUSD 26.08 `usdLod` schemas, inside
`usdGeoUsd` only:

| Schema | Use |
| --- | --- |
| `UsdLodRootAPI` | Applied to each tile prim whose children are LOD items |
| `UsdLodScreenSizeHeuristic` | Authored once and referenced by tile roots |
| `UsdLodOverrideAPI` | Consumed in tests and offline renders, normally authored in a stronger layer |

The exact property names are taken from the schema definitions in the build.
They are verified against the pinned runtime before authoring code depends on
them.

This raises the effective floor for LOD support:

```text
OpenUSD < 26.08:
    Reader libraries may still build where practical.
    Standard LOD authoring is unavailable.

OpenUSD >= 26.08:
    usdLod-based authoring is enabled.
```

No fallback LOD representation is maintained for older runtimes, and no
repository-specific LOD schema is published. See the
[tile and LOD contract](../architecture/lod.md) and the
[development policy](../development-policy.md).

## Possible Additional Surface

Not adopted, and listed so the compatibility impact is visible before it is:

| Mechanism | Impact |
| --- | --- |
| `SDF_FORMAT_ARGS` handling | No new API surface; the plugins parse and normalize arguments themselves |
| `PcpDynamicFileFormatInterface` | Adds a Pcp dependency, `SdfMetadata` field declarations in each manifest, and recomposition behavior to test |

Both plugin manifests currently declare only `bases`, `extensions`,
`formatId`, `primary`, and `target`. Dynamic file format support would extend
them. The decision is open in
[ADR-0003](../roadmap/adr-0003-dynamic-file-format.md), and the runtime's
`PcpDynamicFileFormatInterface` surface is verified before it is taken.

## Host Expectations

- The host discovers plugins through `PXR_PLUGINPATH_NAME`; see the README.
- `Read` requires a writable layer and full point data. `metadataOnly` reads
  are refused.
- Authored stages use up axis `Y` and `metersPerUnit` 1.0 regardless of the
  source CRS units. The source-to-stage relationship is recorded in the
  `geo:*` attributes documented in
  [supported formats](../supported-formats.md).
- Once LOD is authored, the host selects the active LOD. The plugins author the
  hierarchy, heuristics, and default index; they never read a camera or a
  viewport.
