# OST Reports

These reports are append-only records of OpenStrata (`ost`) adoption in this
repository. They preserve the commands that were run, the CI evidence that was
observed, repository-side fixes, and any follow-up asks for OpenStrata.

## Reading Order

The reports record OST adoption and release-readiness work for this repository.

| Report | Date | Subject | OST version | Result |
| --- | --- | --- | --- | --- |
| [02](02-2026-08-14-v0.8.0-release-dogfooding.md) | 2026-08-14 | v0.8.0 release dogfooding, local build/test, and packaging | 0.21.0 local validation | Source build and 21 tests passed; workspace packaging blocked by managed-output digest mismatch |
| [01](01-2026-08-11-v0.22.0-ply-fileformat-ci.md) | 2026-08-11 | v0.22.0 PLY FileFormat readiness, CI, and smoke-fixture arguments | 0.21.0 validation baseline | PLY CI green on Windows, macOS arm64, and Linux after repository fixes; one upstream usability ask is carried into OST v0.22.0 |

Reports are historical evidence. When a later OpenStrata version changes an
observation, add a new report rather than rewriting the old one.
