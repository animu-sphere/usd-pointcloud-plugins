# Building and testing

The full build and test surface for this workspace. The short version is in the
top-level [README](../../README.md#quick-start); installing and registering the
plugins is in [INSTALL.md](INSTALL.md), and redistribution obligations are in
[DISTRIBUTION.md](DISTRIBUTION.md).

Requirements:

- CMake 3.23 or newer
- A C++17 compiler
- OpenStrata `ost` 0.22.7 for the pinned workspace build
- OpenUSD 26.08 for the USD targets, with the plugin contract accepting
  `>=26.08,<27.0`

The verified local runtime is OpenUSD 26.08 on Windows x86-64 / MSVC / Python
3.13, obtained through the pinned `cy2026` / `usd` OpenStrata runtime. See
[OPENUSD.md](../compatibility/OPENUSD.md).

## OpenStrata path (primary)

From the repository root:

```powershell
ost configure
ost build
ost test
```

`ost configure` writes the toolchain, environment, and CMake presets under
`.strata/targets/<target>/` and refreshes `CMakeUserPresets.json`. `ost build`
builds every library and all plugin bundles; `ost test` runs the full CTest
suite:

```text
usdGeoCore_unit
usdPointCloudCore_unit
usdLas_unit
usdLaz_unit
usdPointCloudAuthoring_unit
pointcloudLas_integration
pointcloudLaz_integration
pointcloudCopc_integration
```

The three `*_integration` tests load the built plugin through OpenUSD and check
the authored stage, so they require the OpenUSD runtime. The five `*_unit`
tests cover the libraries; only `usdPointCloudAuthoring_unit` needs OpenUSD.

## Per-bundle builds and verification

Each bundle is independently buildable and verifiable:

```powershell
ost plugin build .\plugins\pointcloud-las
ost plugin build .\plugins\pointcloud-laz
ost plugin build .\plugins\pointcloud-copc
ost plugin doctor .\plugins\pointcloud-las
ost plugin test .\plugins\pointcloud-las --up-to 4
ost plugin test .\plugins\pointcloud-laz --up-to 4
ost plugin test .\plugins\pointcloud-copc --up-to 4
ost plugin package .\plugins\pointcloud-las
```

`ost plugin build` produces the shared library under the bundle's `lib/` and
configures its `plugInfo.json`:

```text
plugins/pointcloud-las/lib/libUsdGeoLasFileFormat.dll
plugins/pointcloud-las/plugin/resources/pointcloud-las/plugInfo.json
```

A per-bundle `ost plugin build` does not define `USDGEO_BUILD_TESTS`, so the
CTest integration targets are built only by the workspace `ost build`. Run both
when changing a plugin.

The bundles do not all declare OST test fixtures yet, so `ost plugin test` currently
reports the L3 `usdcat.read` and L4 `python.stage_open` checks as skipped. That
gap is tracked in
[implementation status](../roadmap/implementation-status.md).

## Explicit tiled conversion

The production path for long-running tiled generation is the workspace
converter. It reuses the LAS/LAZ readers and shared payload authoring path:

```powershell
usd-pointcloud-convert `
  C:\path\to\sample.las `
  C:\path\to\output\PointCloud.usda `
  --tile-size 128 `
  --memory-limit 1048576 `
  --attributes xyz,intensity,classification
```

The output root and payload directory must not already exist. The converter
creates a temporary root layer and publishes it only after tiled authoring
completes. Cancellation cleanup and interrupted-transaction recovery are
validated; use `--help` for the current option surface.

An optional `--cache-root <directory>` enables deterministic USDC cache lookup
and generation. Cache entries contain `root.usdc`, `cache.manifest`, and a
`payloads/` directory keyed by the canonical source identity and normalized
generation arguments. Each generated payload directory also contains the
deterministic `tiles.manifest` tile inventory. Cache-enabled output uses
`payloads/` beside the output root so the cached payload references remain
portable:

```powershell
usd-pointcloud-convert `
  C:\path\to\sample.las `
  C:\path\to\output\PointCloud.usda `
  --tile-size 128 `
  --cache-root C:\path\to\pointcloud-cache
```

An existing committed cache entry is materialized without decoding the source
again. The cache is derived data; the source file and normalized arguments
remain the authority for invalidation.

Direct LAS, LAZ, and COPC FileFormat reads can use the same cache entries by
setting `USDGEO_CACHE_ROOT` in the host process. A committed hit loads the
cached root before point decoding. The environment variable is storage
configuration only and is not part of the file-format argument map.

```powershell
$env:USDGEO_CACHE_ROOT = 'C:\path\to\pointcloud-cache'
usdcat 'C:\path\to\sample.las:SDF_FORMAT_ARGS:tile=true&tileSize=128&payloadDirectory=payloads'
```

## Plain CMake path

The libraries build and test without `ost` and, for everything except the
authoring library, without an OpenUSD runtime:

```powershell
cmake -S . -B build -DUSDGEO_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`USDGEO_BUILD_USD` controls whether the OpenUSD authoring library and the three
plugin bundles are added. It defaults to `ON` when `pxr_ROOT` or
`OpenUSD_ROOT` is defined and `OFF` otherwise, so a bare configure builds and
tests only `usdGeoCore`, `usdPointCloudCore`, `usdLas`, and `usdLaz`. Keeping
that path green is a
[workspace contract](../architecture/WORKSPACE.md) requirement.

Building the USD targets this way requires the compiler environment and the
OpenUSD install that `ost configure` prepares; the generated preset is the
supported way to get both:

```powershell
ost configure
cmake --preset cy2026-windows-x86_64-py313-usd
```

## Previewing a file

```powershell
ost plugin view `
  .\plugins\pointcloud-las `
  C:\path\to\sample.las `
  --with .\plugins\pointcloud-laz
```

`--with` makes one session discover the selected bundles. File-format arguments use
the standard USD suffix:

```powershell
ost plugin view `
  .\plugins\pointcloud-las `
  'C:\path\to\sample.las:SDF_FORMAT_ARGS:lod=balanced' `
  --with .\plugins\pointcloud-laz
```

The supported compact profiles are `off`, `preview`, `balanced`, and
`quality`. The full argument surface is in
[FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md).

## CI

`openstrata.ci.yaml` is the cross-platform CI contract; the checked-in GitHub
Actions workflows are generated from it. Regenerate them after any matrix
change:

```sh
ost ci generate github --force
ost ci validate
```

The declared PR matrix and the required local gate are fixed in
[WORKSPACE.md §9](../architecture/WORKSPACE.md).
