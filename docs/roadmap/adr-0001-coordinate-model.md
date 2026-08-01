# ADR 0001: Coordinate Model

- Status: accepted
- Scope: Phase 1

## Decision

Keep the source coordinate reference system and USD stage coordinates as separate concepts. A dataset carries CRS metadata and `localOrigin`; internal point and terrain calculations use `double`.

```text
stored source coordinate
  -> CRS / unit interpretation
  -> subtract explicit localOrigin
  -> stage-local USD coordinate
```

## Rationale

Converting ECEF or large projected coordinates directly to `float` loses local precision. An explicit local origin allows display-side `float` use while preserving reconstruction of source coordinates.

## Open Questions

- How to connect to PROJ
- The ECEF / ENU transformation API
- Whether to allow a separate origin per tile
- Detailed representation of the vertical datum and epoch for Z

Resolve these questions after the Phase 0 validation with a reader and real data.
