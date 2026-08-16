# Diagnostics Contract

Section 8 of the [design policy](../design/DESIGN_POLICY.md) requires typed
diagnostics instead of string-only errors. This document records the current
state and the target contract.

## Current State

Reader APIs (`usdlas`, `usdlaz`) expose typed diagnostic overloads while
keeping the original `std::string& error` overloads for compatibility. The
reader messages are projected into shared diagnostic codes, and both
FileFormat Plugins consume those typed overloads. Plugins retain their stable
user-facing code prefixes and include available byte or point anchors in the
`TF_RUNTIME_ERROR` message:

```text
[LAS005] Unable to inspect LAS file <path>: unsupported LAS point format
```

Codes are owned per plugin and listed in
[pointcloud-las diagnostics](../../plugins/pointcloud-las/docs/DIAGNOSTICS.md) and
[pointcloud-laz diagnostics](../../plugins/pointcloud-laz/docs/DIAGNOSTICS.md).
The existing import-stage codes are fatal, because none of them leave a stage
that can be opened. Resolver-backed cache decisions are recoverable warnings;
COPC reports disabled reuse with `COPC009` and continues by reading and
authoring from the source.

Remaining limitations of the current migration:

- The reader compatibility projection still classifies some legacy messages
   internally; native typed emission should replace it over time.
- Byte offsets and point indices are only present when a message happens to
  include them.
- Most warning conditions still cannot be expressed through the shared typed
   diagnostic value, so they either fail the read or disappear. Resolver cache
   decisions are currently projected through the plugin warning code described
   above.
- Plugin codes remain scoped to the import stage, so one shared condition can
   still be presented under different LAS or LAZ stage codes.

## Target Contract

`usdGeoCore` owns the diagnostic value types so every reader and plugin shares
them.

```cpp
enum class Severity { Warning, Error };

enum class DiagnosticCode {
    InvalidSignature,
    UnsupportedVersion,
    UnsupportedPointFormat,
    TruncatedHeader,
    InvalidOffset,
    InvalidRecordLength,
    TruncatedRecord,
    InvalidCrs,
    ConflictingCrs,
    UnsupportedExtraBytesType,
    MissingWaveformData,
    NonFiniteCoordinate,
   DecodeFailure,
   UnknownFormatArgument,
   UnsupportedFormatArgument,
   InvalidFormatArgument,
   ConflictingFormatArguments
};

struct Diagnostic {
    DiagnosticCode code;
    Severity severity;
    std::string message;
    std::optional<std::uint64_t> byteOffset;
    std::optional<std::uint64_t> pointIndex;
};
```

Rules:

1. `DiagnosticCode` values are stable once published. Names are never reused
   for a different meaning; new conditions get new values.
2. `message` is for humans and may change between releases. Callers must not
   parse it.
3. `byteOffset` is set whenever the condition is anchored to a file position.
4. `pointIndex` is set whenever the condition is anchored to a point record.
5. `Warning` means the read continues with a documented interpretation.
   `Error` means the read stops.
6. Readers collect diagnostics; they do not throw and do not write to stderr.
7. The FileFormat Plugin layer converts diagnostics into OpenUSD diagnostics
   and keeps emitting the existing `LASxxx` / `LAZxxx` prefixes.

## Migration

The plugin-level codes stay as the user-visible contract. They become a
projection of the shared codes rather than an independent taxonomy.

1. Add the value types to `usdGeoCore` with unit tests. **Complete.**
2. Return `std::vector<Diagnostic>` from `usdlas` inspection and decode entry
   points, keeping the existing string overloads until callers migrate.
   **Complete.**
3. Map each `DiagnosticCode` to the existing plugin code, extending the code
   tables only when a condition has no existing code. **Complete.**
4. Move `usdlaz` and both plugins onto the typed path. **Complete.** The
   plugin prefixes remain stable while typed messages and anchors are carried
   through.
5. Remove the string-only overloads once no caller uses them.

Existing `LASxxx` and `LAZxxx` codes keep their meaning through the migration.
Adding a code is a compatible change; changing what a code means is not.

## Planned Codes

The [tile and LOD contract](LOD.md) requires typed diagnostics for every
validation invariant it defines: item ordering, default index range, threshold
count and ordering, non-finite or non-positive thresholds, inconsistent item
bounds, non-monotonic point counts, source-range overflow, and empty
hierarchies. Those codes shipped with LOD validation. New codes follow rule 1
above and are not reserved by name in advance, so that published names match
shipped behavior.

Sampling and tile-generation failures follow the same rule: readers and
generators collect diagnostics, and the authoring layer refuses to author a
hierarchy that fails validation rather than emitting a partial LOD root.

The [streaming and tiling](../roadmap/streaming-and-tiling.md) work adds
conditions this taxonomy does not yet cover — spool write failures, incomplete
spool detection, exhausted open-file budgets, and payload commit failures.
Each gets a new `DiagnosticCode` value and a new plugin code when it ships;
none reuses an existing meaning.

`v0.10.0` adds resolver identity and generated-cache decisions to the same
taxonomy: identity unavailable, unstable, stable, or changed, and cache reuse
disabled, cache hit, or cache invalidated. These explain the decision without
leaking transport specifics or validation-token contents, so a message such as
`Missing HTTP ETag` is not acceptable while
`Generated cache reuse disabled: the active resolver did not provide a stable
source validation identity.` is. The contract is in
[RESOLVER_SOURCE.md](RESOLVER_SOURCE.md); the code names follow rule 1 above
and are not reserved in advance.
