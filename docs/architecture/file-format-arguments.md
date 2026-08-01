# File-Format Argument Contract

Section 9 of the LOD policy allows file-format arguments to control tiling and
LOD generation, and the [library architecture](../roadmap/library-architecture.md)
requires arguments to be normalized before any reader or cache lookup. This
document defines that contract.

Status: **not implemented**. No plugin accepts an argument today, which is why
attribute selection, point limits, and filters are listed as limitations in the
[supported formats](../supported-formats.md).

## Why Arguments Exist

An argument changes what a layer contains. It therefore changes layer identity:
two layers for the same file with different arguments are different layers, and
they must not share a cache entry. Getting this wrong produces stale geometry
that survives an edit, which is worse than having no arguments at all.

Arguments are the only sanctioned way to supply information the source file
does not carry. A missing CRS on a PLY or CSV file is reported as a diagnostic
and can be supplied by an argument; it is never guessed. See the
[development policy](../development-policy.md) section 4.7.

## Syntax

Arguments use the standard OpenUSD encoding:

```usda
def "Survey" (
    references = @./sample.las:SDF_FORMAT_ARGS:lod=balanced&attributes=xyz,rgb@
)
{
}
```

```bash
usdcat "sample.laz:SDF_FORMAT_ARGS:lod=preview"
```

The plugins do not invent a second argument syntax.

## Candidate Arguments

| Argument | Controls | Alters topology |
| --- | --- | --- |
| `lod` | Compact LOD profile: `off`, `preview`, `balanced`, `quality` | Yes |
| `lodLevels` | Number of LOD items per root | Yes |
| `lodPointCounts` | Target point count per LOD item | Yes |
| `lodRatios` | Retention ratio per LOD item | Yes |
| `lodThresholds` | Screen-size thresholds | No |
| `tile` | Whether to partition spatially | Yes |
| `tileDepth` | Partition depth | Yes |
| `tilePointLimit` | Point budget that triggers a split | Yes |
| `sampling` | Sampling algorithm | Yes |
| `attributes` | Attribute subset to author | Yes |
| `classification` | Classification filter | Yes |
| `bounds` | Spatial filter | Yes |
| `originMode` | Local-origin policy | No |
| `upAxis` | Stage up-axis conversion | No |

"Alters topology" means the authored namespace or point count changes, not just
attribute values. An argument in that column must never reuse a cached layer
produced with a different value.

The first implementation may ship only `lod` and `attributes`. Explicit numeric
controls are added once the LOD contract stabilizes, so that the compact
profile does not become an alias for a surface that later changes meaning.

## Rules

1. **Validate everything.** An unknown key, an unparsable value, or an
   out-of-range value produces a typed diagnostic. Unknown keys are not
   silently ignored, because a typo would otherwise be indistinguishable from a
   default.
2. **Defaults are deterministic** and documented. An absent argument and an
   argument set to its default value produce identical output.
3. **Normalize before use.** Key order, whitespace, numeric formatting, and
   list order are normalized into one canonical form before the reader or the
   cache sees them.
4. **Normalized arguments participate in layer identity and cache keys.**
   Equivalent arguments that normalize to the same form share a cache entry;
   arguments that do not, do not.
5. **Unsupported combinations produce stable diagnostics**, not a silently
   narrowed result. `lod=off` combined with `lodLevels=3` is a conflict, not a
   precedence puzzle.
6. **Arguments never replace the standard USD representation.** They control
   generation. LOD is still authored as `usdLod`; see the
   [tile and LOD contract](lod.md).
7. **Normalization lives in the plugin, validation in the shared layer.** The
   plugin parses `SDF_FORMAT_ARGS` into a project-owned options struct; the
   readers and `usdGeoUsd` validate that struct and know nothing about Sdf
   argument encoding.

## Relationship to Read Options

`PointReadOptions` already carries `chunkPointLimit`, `memoryBudgetBytes`,
`range`, and `isCancelled`; see
[point reader architecture](point-reader.md). Arguments are how a host reaches
those fields. They are unreachable today only because the plugins do not
normalize arguments or pass options, as recorded in the
[plugin adapter contract](plugin-adapter.md).

Not every read option becomes an argument. `isCancelled` is a host-supplied
callback and never appears in a layer identifier. A memory budget is a
candidate but is a host policy rather than a property of the asset, so it
should not silently change authored output.

## Testing

- Equivalent arguments in different orders normalize to one cache key.
- Different topology-altering arguments never collide in the cache.
- Unknown keys and out-of-range values produce stable diagnostic codes.
- Conflicting combinations are rejected rather than resolved by precedence.
- A default-valued argument and an absent argument produce identical layers.
- LAS and LAZ accept the same arguments with the same meaning.
