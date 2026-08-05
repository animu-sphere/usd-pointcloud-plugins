# Migration

Breaking changes to names, paths, and identifiers, and what to do about them.
The project is pre-1.0, so cleanup is appropriate — but every migration is
recorded here explicitly rather than left for a consumer to discover.

## Unreleased: repository rename

The GitHub repository was renamed from `animu-sphere/usd-geo-plugins` to
`animu-sphere/usd-pointcloud-plugins`. This is a repository and documentation
identity change only; it does not rename libraries, CMake targets, plugin
bundles, USD attributes, diagnostics, or file-format arguments.

Update local remotes and any external links:

```powershell
git remote set-url origin https://github.com/animu-sphere/usd-pointcloud-plugins.git
```

GitHub redirects the old repository URL after the rename, but badges, clone
instructions, release URLs, and automation should use the new URL.

The `geospatial-las` and `geospatial-laz` bundle names remain unchanged in
this migration. Bundle renaming, if needed, is a separate compatibility event.

## Unreleased: module and bundle rename

The point-cloud authoring library and both plugin bundles were renamed so that
their names state their responsibility. This change is name-only: no behavior,
authored output, diagnostic code, or file-format argument changed with it.

### Directories

```text
libs/usd-geo-usd    -> libs/usd-pointcloud-authoring
plugins/geo-las     -> plugins/geospatial-las
plugins/geo-laz     -> plugins/geospatial-laz
```

### Bundle and library identities

| Old | New |
| --- | --- |
| bundle `geo-las` | bundle `geospatial-las` |
| bundle `geo-laz` | bundle `geospatial-laz` |
| library `usdGeoUsd` | library `usdPointCloudAuthoring` |
| CMake package `usdGeoUsd` | CMake package `usdPointCloudAuthoring` |
| CMake target `usdgeo::usd` | CMake target `usdpointcloud::authoring` |
| CMake target `GeoLasFileFormat` | CMake target `UsdGeoLasFileFormat` |
| CMake target `GeoLazFileFormat` | CMake target `UsdGeoLazFileFormat` |
| shared library `GeoLasFileFormat` | shared library `UsdGeoLasFileFormat` |
| shared library `GeoLazFileFormat` | shared library `UsdGeoLazFileFormat` |
| `plugInfo.json` type `GeoLasFileFormat` | `plugInfo.json` type `UsdGeoLasFileFormat` |
| `plugInfo.json` type `GeoLazFileFormat` | `plugInfo.json` type `UsdGeoLazFileFormat` |
| resources `plugin/resources/geo-las/` | resources `plugin/resources/geospatial-las/` |
| resources `plugin/resources/geo-laz/` | resources `plugin/resources/geospatial-laz/` |
| headers `include/geolas/`, `include/geolaz/` | headers `include/usdgeolas/`, `include/usdgeolaz/` |
| C++ `geolas::diagnostics`, `geolaz::diagnostics` | C++ `usdgeolas::diagnostics`, `usdgeolaz::diagnostics` |

The external bundle name uses the explicit `geospatial` term while the internal
C++ prefix stays `UsdGeo`, to avoid excessively long symbols. See
[WORKSPACE.md §3](../architecture/WORKSPACE.md).

### What did not change

- The `usdgeo`, `usdpointcloud`, `usdlas`, and `usdlaz` C++ namespaces.
- The authoring library's public header path, `include/usdgeo/`.
- The `usdgeo::core`, `usdpointcloud::core`, `usdlas::core`, and `usdlaz::core`
  CMake targets.
- The registered file-format ids `las` and `laz` and the `.las` / `.laz`
  extensions. Existing layers and references do not change.
- Every `LASxxx` and `LAZxxx` diagnostic code.
- Every file-format argument name and value.
- The authored stage: prim path, prim type, attribute names, and metadata.

### CMake consumers

`usdgeo::usd` remains as a deprecated alias of `usdpointcloud::authoring`:

```cmake
# still works, removed in v0.3.0
target_link_libraries(my_target PRIVATE usdgeo::usd)

# preferred
target_link_libraries(my_target PRIVATE usdpointcloud::authoring)
```

There is no alias for the bundle directories or the plugin CMake targets,
because a directory rename cannot be aliased.

### Plugin discovery

`PXR_PLUGINPATH_NAME` values that named the old resource directory must be
updated:

```powershell
# before
$env:PXR_PLUGINPATH_NAME = "C:\path\to\geo-las\plugin\resources\"
# after
$env:PXR_PLUGINPATH_NAME = "C:\path\to\geospatial-las\plugin\resources\"
```

```bash
# before
export PXR_PLUGINPATH_NAME=/path/to/geo-las/plugin/resources/
# after
export PXR_PLUGINPATH_NAME=/path/to/geospatial-las/plugin/resources/
```

Because the trailing-slash form makes OpenUSD search subdirectories, a path
that pointed at the parent `plugin/resources/` directory keeps working.

### OpenStrata commands

```powershell
# before
ost plugin build .\plugins\geo-las
ost plugin test .\plugins\geo-laz --up-to 4
# after
ost plugin build .\plugins\geospatial-las
ost plugin test .\plugins\geospatial-laz --up-to 4
```

Packaged artifact names follow the bundle name, so
`geo-las-<version>-<target>.tar.zst` becomes
`geospatial-las-<version>-<target>.tar.zst`. Installed bundle names are treated
as a compatibility surface: a future rename gets the same treatment as this one.

### Checklist for a consumer

1. Update `PXR_PLUGINPATH_NAME` and any hard-coded bundle path.
2. Update `ost plugin *` invocations and CI bundle paths.
3. Update any script that names `GeoLasFileFormat.dll` or
   `GeoLazFileFormat.dll` directly.
4. Switch CMake links from `usdgeo::usd` to `usdpointcloud::authoring` before
   v0.3.0.
5. Nothing else. Layers, references, format arguments, and authored output are
   unaffected.
