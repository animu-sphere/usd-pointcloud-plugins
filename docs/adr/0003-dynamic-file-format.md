# ADR-0003: Dynamic File Format Plugins

## Status

Accepted for the format-specific `pc_<format>_lod` fields (2026-08-11). Other generation parameters
remain static file-format arguments until their recomposition and cache costs
are measured.

## Context

The plugins are static `SdfFileFormat` implementations. A layer's content is a
function of the resolved path plus its `SDF_FORMAT_ARGS`, and arguments can
only be supplied by baking them into the asset path:

```usda
def "Survey" (
    references = @./sample.las:SDF_FORMAT_ARGS:lod=balanced@
)
{
}
```

Tiling and LOD add parameters that a user will want to change per shot, per
department, or interactively: LOD profile, tile depth, point budget, attribute
subset, classification filter. Baking those into asset paths means editing the
path string to change a quality setting, and every distinct setting is a
distinct asset path in the composed scene.

OpenUSD provides `PcpDynamicFileFormatInterface` for this. A dynamic file
format composes its file-format arguments from metadata authored on the prim
that introduces the payload, so a parameter becomes an authored field rather
than part of an asset path:

```usda
def "Survey" (
    prepend payload = @./sample.las@
)
{
    pc_las_lod = "balanced"
}
```

Changing `pc_las_lod` recomposes the payload. The custom fields are declared in
the plugin's `plugInfo.json` under `SdfMetadata`, the format declares dynamic
support, and the implementation provides
`ComposeFieldsForFileFormatArguments` and
`CanFieldChangeAffectFileFormatArguments`. The LAS, LAZ, and COPC manifests
declare a format-specific `pc_<format>_lod` prim metadata field, and each
format maps it to the existing normalized `lod` argument. The names are
format-specific because OpenUSD registers plugin metadata fields globally.

## Options

### A. Stay static, arguments only

Ship the [file-format argument contract](../architecture/FILE_FORMAT_ARGUMENTS.md)
and nothing else. Parameters live in asset paths.

- Simplest, and it is a prerequisite for the other two options regardless.
- Changing a parameter means editing an asset path, which is awkward for
  interactive LOD tuning and noisy in version control.
- Sufficient if LOD parameters are set once at ingest and rarely revised.

### B. Static plugin plus a generated tile asset

Generate a USDC cache with the tile and LOD hierarchy, and reference that
instead of re-reading the source. Parameters become inputs to a generation
step, not to layer composition.

- Matches the payload-partitioned target in the
  [tile and LOD contract](../architecture/LOD.md), where each LOD child is its
  own `.usdc`.
- Best runtime behavior for large data: no re-decode, real payload boundaries.
- Requires the generated USDC cache path to be stable and usable by hosts. The
  repository now provides deterministic cache descriptors, generation through
  the conversion tool, and direct lookup through `USDGEO_CACHE_ROOT`.
- Parameters are not interactively adjustable on the composed stage.

### C. Dynamic file format

Implement `PcpDynamicFileFormatInterface` so parameters are authored fields.

- Parameters are editable in the scene, overridable per layer, and inspectable
  by tooling without parsing asset paths.
- Field changes trigger recomposition, which for a point cloud means re-reading
  and re-authoring. Without the cache from option B, an interactive change can
  re-decode a large file. `CanFieldChangeAffectFileFormatArguments` must be
  precise, or unrelated edits cause expensive recomposition.
- Adds a Pcp-level dependency and a set of project-owned metadata fields to the
  public surface. Those fields become a compatibility commitment.
- Interacts with LOD in a way that needs care: `usdLod` selection is a runtime
  concern handled by the renderer, while a dynamic argument changes what is
  composed. Exposing both as similar-looking authored fields risks users
  reaching for the wrong one.

## Decision

Keep A and B as the compatibility and production paths, and adopt a narrow form
of C for LOD profile selection: **A, then B, then `pc_<format>_lod`**.

A is required work under any option. B delivers the actual large-data win and
is already the documented target structure. C is attractive for authoring
ergonomics but its cost is dominated by recomposition, which B is what makes
affordable.

The initial dynamic fields are deliberately limited to the compact `lod` profile.
When `USDGEO_CACHE_ROOT` contains a committed entry, recomposition loads the
derived USDC result; a cache miss follows the existing reader path.

## Open Questions

1. Which parameters would be dynamic fields, and which stay arguments? A
   parameter that alters authored topology is a poor candidate for casual
   interactive editing.
2. What is the recomposition cost for a representative file once the USDC cache
   exists, and does `CanFieldChangeAffectFileFormatArguments` isolate it
   adequately?
3. How do dynamic argument fields coexist with `UsdLodOverrideAPI` without
   presenting two overlapping ways to control detail?
4. Do the `geo:*` metadata field names collide with the authored `geo:*`
   attributes already documented in
   [capability matrix](../reference/CAPABILITY_MATRIX.md)? A distinct namespace is
  required; `pc_<format>_lod` is intentionally outside the `geo:*` namespace.

## Consequences If Adopted

- All three plugin manifests declare their format-specific `pc_<format>_lod`
  `SdfMetadata` field.
- The field names become a stable public contract, versioned like diagnostic
  codes.
- Argument normalization is shared between the static argument path and the
  composed-field path, so one file cannot mean two things depending on how the
  parameter arrived.
- The LAS integration test covers payload recomposition into a `usdLod` root;
  LAZ and COPC use the same adapter contract and are covered by bundle smoke
  tests and plugin builds.
