# Installing and registering the plugins

The latest tagged release is v0.2.2
([release records](../releases/README.md)); `main` carries unreleased work.
Building from source is in [BUILDING.md](BUILDING.md), and redistribution
obligations — which matter for `pointcloud-laz` — are in
[DISTRIBUTION.md](DISTRIBUTION.md).

Check [OPENUSD.md](../compatibility/OPENUSD.md) before reusing a binary:
OpenUSD plugin binaries must match the target platform, compiler ABI, and
OpenUSD build of the host application.

The workspace ships two independent bundles: `pointcloud-las` for `.las` and
`pointcloud-laz` for `.laz`. Neither depends on the other; install only what
you need. `pointcloud-laz` contains LGPL-2.1 laz-perf code and
`pointcloud-las` does not, so the two have different redistribution
obligations.

## Bundle layout

An installed bundle has this layout:

```text
<bundle>/lib/UsdGeoLasFileFormat.dll        # or .so / .dylib
<bundle>/plugin/resources/pointcloud-las/plugInfo.json
<bundle>/openstrata.plugin.yaml
```

The LAZ bundle is the same shape with `UsdGeoLazFileFormat` and
`plugin/resources/pointcloud-laz/`.

## Registering with OpenUSD

Point `PXR_PLUGINPATH_NAME` at the directory that holds `plugInfo.json`. A
trailing slash makes OpenUSD search subdirectories, which registers both
bundles at once:

```powershell
$env:PXR_PLUGINPATH_NAME = "C:\path\to\pointcloud-las\plugin\resources\"
```

```bash
export PXR_PLUGINPATH_NAME=/path/to/pointcloud-las/plugin/resources/
```

The plugin library must be able to load the OpenUSD libraries it was built
against, so keep the runtime used for the build on `PATH` or
`LD_LIBRARY_PATH`.

If you are migrating from v0.1.0, the bundle directory and `plugInfo.json`
resource directory both changed name; see
[MIGRATION.md](../compatibility/MIGRATION.md).

## Opening a file

Any OpenUSD application that discovers FileFormat Plugins can open a LAS or
LAZ path directly:

```bash
usdview sample.las
usdcat sample.laz
usdcat --flatten sample.las -o sample.usda
```

Or reference one from a layer:

```usda
def "Survey" (
    references = @./sample.las@
)
{
}
```

With file-format arguments:

```usda
def "Survey" (
    references = @./sample.las:SDF_FORMAT_ARGS:lod=balanced&attributes=xyz,rgb@
)
{
}
```

## Without a separate install

`ost plugin view` and `ost plugin run` compose the runtime and plugin
environment for a bundle in place, which is the quickest way to check an
installation-free build:

```powershell
ost plugin view `
  .\plugins\pointcloud-las `
  C:\path\to\sample.las `
  --with .\plugins\pointcloud-laz
```

## What you get

A read authors `/PointCloud` as `UsdGeomPoints` on a Y-up, one-meter-per-unit
stage, with positions as stage-local `float` values and the source origin in
`geo:localOrigin`. The complete authored shape, the attribute list, and the
`usdLod` variants the compact `lod` profiles produce are in
[CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md).

Rendering is the consuming application's responsibility. These plugins provide
import and USD authoring, not a point-cloud renderer, and LOD selection stays
with the host; see [LOD.md](../architecture/LOD.md).
