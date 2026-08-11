# ADR-0003: Dynamic File Format Plugins

## Status

Proposed. Not decided, and not scheduled before the plugins are thinned onto a
shared reader and authoring path.

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
    int geo:lodLevels = 3
    token geo:lod = "balanced"
}
```

Changing `geo:lod` recomposes the payload. The custom fields are declared in
the plugin's `plugInfo.json` under `SdfMetadata`, the format declares dynamic
support, and the implementation provides
`ComposeFieldsForFileFormatArguments` and
`CanFieldChangeAffectFileFormatArguments`. Neither plugin's manifest declares
anything of the sort today; both register only `bases`, `extensions`,
`formatId`, `primary`, and `target`.

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

## Recommendation

Sequence rather than choose: **A, then B, then reconsider C.**

A is required work under any option, and it is currently blocked only by the
plugin-thinning migration. B delivers the actual large-data win and is already
the documented target structure. C is attractive for authoring ergonomics but
its cost is dominated by recomposition, which B is what makes affordable.

Deciding C before the cache exists risks shipping a mechanism whose main effect
is making it easy to trigger a full re-decode.

## Open Questions

These are resolved before this ADR moves to Accepted or Rejected:

1. Does the pinned OpenUSD 26.08 runtime expose
   `PcpDynamicFileFormatInterface` with the same surface used here? Verify
   alongside the `usdLod` schema check in
   [phase 0](../roadmap/phase-0-technical-validation.md).
2. Which parameters would be dynamic fields, and which stay arguments? A
   parameter that alters authored topology is a poor candidate for casual
   interactive editing.
3. What is the recomposition cost for a representative file once the USDC cache
   exists, and does `CanFieldChangeAffectFileFormatArguments` isolate it
   adequately?
4. How do dynamic argument fields coexist with `UsdLodOverrideAPI` without
   presenting two overlapping ways to control detail?
5. Do the `geo:*` metadata field names collide with the authored `geo:*`
   attributes already documented in
   [capability matrix](../reference/CAPABILITY_MATRIX.md)? A distinct namespace is
   probably required.

## Consequences If Adopted

- Both plugin manifests declare dynamic support and their `SdfMetadata` fields.
- The field names become a stable public contract, versioned like diagnostic
  codes.
- Argument normalization is shared between the static argument path and the
  composed-field path, so one file cannot mean two things depending on how the
  parameter arrived.
- Plugin integration tests cover recomposition on field change, and confirm
  that unrelated field edits do not recompose.
