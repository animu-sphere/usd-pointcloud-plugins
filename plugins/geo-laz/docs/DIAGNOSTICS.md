# geo-laz diagnostics

Every import-time diagnostic has a stable `LAZxxx` code. The code is a
machine-readable prefix; the remainder of the message is human-readable.

All current codes have `FATAL` severity because the LAZ stage cannot be
opened or authored when they are emitted.

Codes are stable. Adding a code is a compatible change; changing what a code
means is not. The typed diagnostics migration keeps these codes as the
user-visible contract; see the
[diagnostics contract](../../../docs/architecture/diagnostics.md).

LAZ decoding delegates record interpretation to the LAS reader, so a decode
failure inside a chunk is reported as `LAZ003` even when its cause is a LAS
record condition.

| Code | Severity | Source | Description |
| --- | --- | --- | --- |
| LAZ001 | FATAL | import | Read requires a writable layer and full point data |
| LAZ002 | FATAL | import | LAZ file could not be opened |
| LAZ003 | FATAL | import | LAZ data could not be decoded |
| LAZ004 | FATAL | import | LAZ bounds could not be transformed to USD |
| LAZ005 | FATAL | import | USD layer could not be created |
| LAZ006 | FATAL | import | USD stage metrics could not be set |
| LAZ007 | FATAL | import | LAZ point cloud could not be authored |