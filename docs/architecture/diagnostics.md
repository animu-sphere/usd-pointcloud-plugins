# Diagnostics Contract

Section 8 of the [development policy](../development-policy.md) requires typed
diagnostics instead of string-only errors. This document records the current
state and the target contract.

## Current State

Reader APIs (`usdlas`, `usdlaz`) return `bool` and fill a `std::string& error`.
The plugin layer wraps that string with a stable code and reports it through
`TF_RUNTIME_ERROR`:

```text
[LAS005] Unable to inspect LAS file <path>: unsupported LAS point format
```

Codes are owned per plugin and listed in
[geo-las diagnostics](../../plugins/geo-las/docs/DIAGNOSTICS.md) and
[geo-laz diagnostics](../../plugins/geo-laz/docs/DIAGNOSTICS.md). Every current
code is fatal, because none of them leave a stage that can be opened.

Limitations of the current model:

- Reader-level failures cannot be classified without parsing English text.
- Byte offsets and point indices are only present when a message happens to
  include them.
- Warnings cannot be expressed, so a recoverable condition either fails the
  read or disappears.
- The same underlying condition can produce different plugin codes depending on
  the call site.

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
    DecodeFailure
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

1. Add the value types to `usdGeoCore` with unit tests.
2. Return `std::vector<Diagnostic>` from `usdlas` inspection and decode entry
   points, keeping the existing string overloads until callers migrate.
3. Map each `DiagnosticCode` to the existing plugin code, extending the code
   tables only when a condition has no existing code.
4. Move `usdlaz` and both plugins onto the typed path.
5. Remove the string-only overloads once no caller uses them.

Existing `LASxxx` and `LAZxxx` codes keep their meaning through the migration.
Adding a code is a compatible change; changing what a code means is not.
