# Release records

Each tagged version receives an immutable record here: what shipped, the
supported behavior, build requirements, known limitations, and licensing notes.
Release records are history and are not rewritten after publication.

| Version | Date | Record |
| --- | --- | --- |
| v0.1.0 | 2026-08-01 | [v0.1.0.md](v0.1.0.md) — shared geospatial and point-cloud contracts, LAS and LAZ readers, and the OpenUSD FileFormat Plugin integration |
| v0.2.0 | 2026-08-05 | [v0.2.0.md](v0.2.0.md) — LAS 1.4 and Extra Bytes coverage, LOD authoring, metadata-only reads, and bounded-memory tiled streaming |

Create a record only once its tag exists: it pins the tagged commit, the
consumed runtime digests, and the published artifact checksums, none of which
are known before the release lane runs.

Unreleased work on `main` is tracked in the root
[CHANGELOG.md](../../CHANGELOG.md) and, at task granularity, in
[roadmap/implementation-status.md](../roadmap/implementation-status.md).

## Release gate

A release record is created only after:

1. `VERSION`, the tag, and the finalized changelog version agree;
2. every declared hosted CI cell in `openstrata.ci.yaml` passes;
3. package digests are reproducible for an unchanged build;
4. notices, SBOM/provenance policy, and target metadata are verified —
   including the LGPL-2.1 obligations that apply to `geospatial-laz`, per
   [DISTRIBUTION.md](../guides/DISTRIBUTION.md);
5. the release is assembled as a draft for human review.

Item 4's document-staging half is not yet machine-enforced and blocks the first
public binary release; see
[DISTRIBUTION.md](../guides/DISTRIBUTION.md).

## How the gate runs

Pushing a `vX.Y.Z` tag starts
[release.yml](../../.github/workflows/release.yml), which validates the tag
against `VERSION`, configures the core CMake targets, runs CTest, and publishes
a source archive with SHA-256 sums.

The workflow takes its runtime digests, `ost` version, and per-cell levels from
`openstrata.ci.yaml` rather than restating them, so re-pinning a runtime moves
the PR and release lanes together.

The v0.2.0 record is finalized for the tagged commit and is not rewritten
after publication.
