# geo-las diagnostics

Every import-time diagnostic has a stable `LASxxx` code. The code is a
machine-readable prefix; the remainder of the message is human-readable.

All current codes have `FATAL` severity because the LAS stage cannot be
opened or authored when they are emitted.

Codes are stable. Adding a code is a compatible change; changing what a code
means is not. The typed diagnostics migration keeps these codes as the
user-visible contract; see the
[diagnostics contract](../../../docs/architecture/diagnostics.md).

| Code | Severity | Source | Description |
| --- | --- | --- | --- |
| LAS001 | FATAL | import | Read requires a writable layer and full point data |
| LAS002 | FATAL | import | LAS file could not be opened |
| LAS003 | FATAL | import | LAS file size could not be determined |
| LAS004 | FATAL | import | LAS header could not be read |
| LAS005 | FATAL | import | LAS header is invalid |
| LAS006 | FATAL | import | LAS VLR metadata is invalid |
| LAS007 | FATAL | import | LAS EVLR offset is outside the file |
| LAS008 | FATAL | import | LAS EVLR metadata is invalid |
| LAS009 | FATAL | import | LAS point data is truncated |
| LAS010 | FATAL | import | LAS point data could not be reached |
| LAS011 | FATAL | import | LAS point could not be read |
| LAS012 | FATAL | import | LAS point could not be decoded |
| LAS013 | FATAL | import | LAS bounds could not be transformed to USD |
| LAS014 | FATAL | import | USD layer could not be created |
| LAS015 | FATAL | import | USD stage metrics could not be set |
| LAS016 | FATAL | import | LAS point cloud could not be authored |