# USD Point Cloud Plugins documentation

Documentation is organized by responsibility, following the same taxonomy as
`usd-3dgs-plugins`, so current contracts, procedures, plans, and historical
records do not drift into one another.

When a summary disagrees with the implementation, the implementation wins and
the summary is a documentation bug. When a summary disagrees with
[architecture/WORKSPACE.md](architecture/WORKSPACE.md) about structure, the
workspace contract wins; structural changes must update that contract first.

| Category | Answers | Start here |
| --- | --- | --- |
| [architecture/](architecture/) | How the workspace is structured, which dependency directions are legal, and what each cross-cutting contract requires. | [WORKSPACE.md](architecture/WORKSPACE.md) |
| [reference/](reference/) | What LAS and LAZ input is accepted today and how it maps to USD. | [CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md) |
| [guides/](guides/) | How to build, test, install, and redistribute the plugins. | [BUILDING.md](guides/BUILDING.md), [INSTALL.md](guides/INSTALL.md) |
| [compatibility/](compatibility/) | Which OpenUSD and OpenStrata versions are supported, and how to migrate across renames. | [OPENUSD.md](compatibility/OPENUSD.md), [MIGRATION.md](compatibility/MIGRATION.md) |
| [roadmap/](roadmap/) | What remains incomplete and in what order it lands. | [README.md](roadmap/README.md) |
| [releases/](releases/) | Immutable records for tagged releases. | [README.md](releases/README.md) |
| [design/](design/) | Why the project is built this way. | [DESIGN_POLICY.md](design/DESIGN_POLICY.md) |
| [adr/](adr/) | Numbered, immutable architecture decision records. | [0001-coordinate-model.md](adr/0001-coordinate-model.md) |
| [contributing/](contributing/) | Contributor procedures that a code change must satisfy. | [MODULE_README_CONTRACT.md](contributing/MODULE_README_CONTRACT.md) |

## Canonical documents

- [design/DESIGN_POLICY.md](design/DESIGN_POLICY.md) defines product intent,
  format scope, coordinate policy, tiling and LOD policy, diagnostics, binary
  safety, testing, and licensing.
- [architecture/WORKSPACE.md](architecture/WORKSPACE.md) is the binding
  structural contract for modules, bundles, dependency directions, and
  artifact naming. A structural change updates it first.
- [reference/CAPABILITY_MATRIX.md](reference/CAPABILITY_MATRIX.md) describes
  what the current tree implements, not what it intends to implement later.
  It separates authoring-library capability from what direct LAS/LAZ
  FileFormat reads reach.
- [architecture/LOD.md](architecture/LOD.md) fixes OpenUSD 26.08 `usdLod` as
  the only public LOD representation and defines tiling, sampling, cache-key,
  and payload rules.
- [architecture/POINT_READER.md](architecture/POINT_READER.md) and
  [architecture/FILE_FORMAT_ARGUMENTS.md](architecture/FILE_FORMAT_ARGUMENTS.md)
  define the shared read contract and the argument surface that reaches it.
- [architecture/PLUGIN_ADAPTER.md](architecture/PLUGIN_ADAPTER.md) is the
  thin-adapter rule every FileFormat Plugin is held to.
- [architecture/DIAGNOSTICS.md](architecture/DIAGNOSTICS.md) defines the typed
  diagnostic contract and its projection onto stable `LASxxx` / `LAZxxx`
  plugin codes.
- [contributing/MODULE_README_CONTRACT.md](contributing/MODULE_README_CONTRACT.md)
  makes each module's `README.md` part of that module's contract rather than
  optional supplementary documentation.

## Component documentation

Component-specific usage stays with the component:

| Component | Kind | Documentation |
| --- | --- | --- |
| `pointcloud-las` | plugin bundle | [plugin README](../plugins/pointcloud-las/README.md) |
| `pointcloud-laz` | plugin bundle | [plugin README](../plugins/pointcloud-laz/README.md) |
| `usdGeoCore` | library | [library README](../libs/usd-geo-core/README.md) |
| `usdGeoCache` | library | [library README](../libs/usd-geo-cache/README.md) |
| `usdPointCloudCore` | library | [library README](../libs/usd-pointcloud-core/README.md) |
| `usdPointCloudAuthoring` | library | [library README](../libs/usd-pointcloud-authoring/README.md) |
| `usdLas` | library | [library README](../libs/usd-las/README.md) |
| `usdLaz` | library | [library README](../libs/usd-laz/README.md) |

Per-bundle diagnostic code tables live with their bundle:
[pointcloud-las](../plugins/pointcloud-las/docs/DIAGNOSTICS.md),
[pointcloud-laz](../plugins/pointcloud-laz/docs/DIAGNOSTICS.md).
