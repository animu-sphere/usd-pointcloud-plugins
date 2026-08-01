# Point Reader Architecture

`usdPointCloudCore` owns the common `PointReadOptions` contract used by the
LAS and LAZ readers.

## Read Options

- `chunkPointLimit` is the requested maximum number of points delivered to a
  consumer at once.
- `memoryBudgetBytes` limits the reader's point and temporary record buffers.
  The effective chunk size is the smaller of the requested limit and the
  budget-derived limit.
- `range.firstPoint` selects the first source point. A zero
  `range.pointCount` selects all remaining points; a nonzero count must fit
  within the header point count.
- `isCancelled`, when provided, is checked before each chunk. A true result
  stops the read with a diagnostic.

Consumers receive only points in the selected range, while readers still
validate the complete source point count. A rejected chunk or malformed point
stops the read.

## Source Access

The LAS reader reads only the header, metadata ranges, and bounded point-record
ranges needed for each chunk. The LAZ decoder remains sequential because the
bundled codec does not expose compressed point seeking; an LAZ point range
therefore avoids delivery of unselected points but still decodes preceding
chunks.

The plugin authoring path can continue to accumulate all delivered points for
the current `UsdGeomPoints` output. Tile, LOD, and cache work will consume the
same reader contract without changing format-specific point decoding.

## Reachability

`usdlas::LasReader` and `usdlaz::LazReader` implement this contract, and both
plugins now pass normalized `chunkPointLimit`, `memoryBudgetBytes`, and `range`
values to them. `isCancelled` remains a host-supplied callback and is not a
file-format argument. See the
[plugin adapter contract](plugin-adapter.md) and the
[file-format argument contract](file-format-arguments.md).

## Relationship to LOD

`range` is the same selection concept the LOD contract calls
`PointSourceRange`. LOD item generation reads each item through this contract
rather than adding a second source-access path, so a sampled item is defined by
a source range plus a deterministic sampling algorithm and version.

The reader never learns about LOD indices, heuristics, cameras, or viewport
state. It delivers validated points for a requested range; the mapping onto
`usdLod` happens in `usdGeoUsd`. See the
[tile and LOD contract](lod.md).