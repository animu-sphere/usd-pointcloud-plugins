# Release records

Each tagged version receives an immutable record here: what shipped, the
supported behavior, build requirements, known limitations, and licensing notes.
Release records are history and are not rewritten after publication.

| Version | Date | Record |
| --- | --- | --- |
| v0.1.0 | 2026-08-01 | [v0.1.0.md](v0.1.0.md) — shared point-cloud contracts, LAS and LAZ readers, and the OpenUSD FileFormat Plugin integration |
| v0.2.0 | 2026-08-05 | [v0.2.0.md](v0.2.0.md) — LAS 1.4 and Extra Bytes coverage, LOD authoring, metadata-only reads, and bounded-memory tiled streaming |
| v0.2.1 | 2026-08-06 | [v0.2.1.md](v0.2.1.md) — point filters, EPSG/CRS resolution, conversion manifests, and tiled cleanup validation |
| v0.2.2 | 2026-08-06 | [v0.2.2.md](v0.2.2.md) — corrected package and product version metadata; no runtime behavior changes |
| v0.3.0 | 2026-08-11 | [v0.3.0.md](v0.3.0.md) — local COPC reading, native hierarchy LOD authoring, and deterministic cache reuse |
| v0.4.0 | 2026-08-12 | [v0.4.0.md](v0.4.0.md) — bounded PLY point streaming, adaptive display attributes, shared authoring, and payload-backed tiled reads |
| v0.5.0 | 2026-08-12 | [v0.5.0.md](v0.5.0.md) — resolver-backed COPC reads, source identity, and cross-platform resolver CI |
| v0.6.0 | 2026-08-12 | [v0.6.0.md](v0.6.0.md) — cache lookup states, statistics, recovery, and reuse baselines |
| v0.7.0 | 2026-08-13 | [v0.7.0.md](v0.7.0.md) — adaptive point-budget tiling, fixed-grid compatibility, and cross-format benchmarks |
| v0.8.0 | 2026-08-14 | [v0.8.0.md](v0.8.0.md) — real-world fixed/adaptive baselines, I/O observability, and LAZ point-format-7 hardening |
| v0.9.0 | 2026-08-15 | [v0.9.0.md](v0.9.0.md) — TilePlan convergence, COPC-native planning, and interactive host-responsiveness validation |
| v0.11.0 | 2026-08-27 | [v0.11.0.md](v0.11.0.md) — composable aggregate product and packaged local/Tier 2 acceptance probes |
| v0.10.0 | 2026-08-23 | [v0.10.0.md](v0.10.0.md) — generated-cache decision diagnostics, a revision-aware cache layout, Windows workspace CTest runtime coverage, Tier 1 as a CI gate, and recorded external resolver interoperability |

Prepare the record in the release commit immediately before creating its tag.
The tag pins the source commit and the record pins the release scope; runtime
digests and published artifact checksums are appended to the generated release
notes by the release workflow. Release records are not rewritten after
publication.

Unreleased work on `main` is tracked in the root
[CHANGELOG.md](../../CHANGELOG.md) and, at task granularity, in
[roadmap/implementation-status.md](../roadmap/implementation-status.md).

## Release gate

A release record is created only after:

1. `VERSION`, `openstrata.toml`, all plugin manifests, all plugin CMake
   projects, the tag, and the finalized changelog version agree;
2. every declared hosted CI cell in `openstrata.ci.yaml` passes;
3. package digests are reproducible for an unchanged build;
4. notices, SBOM/provenance policy, and target metadata are verified —
   including the LGPL-2.1 obligations that apply to `pointcloud-laz` and
   `pointcloud-copc`, per
   [DISTRIBUTION.md](../guides/DISTRIBUTION.md);
5. the release is assembled as a draft for human review.

The workflow stages and existence-checks the required documents before
generating `SHA256SUMS`; a qualified reviewer still confirms the licensing
interpretation before publication. See
[DISTRIBUTION.md](../guides/DISTRIBUTION.md).

## How the gate runs

Pushing a `vX.Y.Z` tag starts
[release.yml](../../.github/workflows/release.yml), which validates the tag
against `VERSION`, configures the core CMake targets, runs CTest, and publishes
a source archive with SHA-256 sums.

The workflow takes its runtime digests, `ost` version, and per-cell levels from
`openstrata.ci.yaml` rather than restating them, so re-pinning a runtime moves
the PR and release lanes together.

## Release procedure

Run these steps from the repository root before creating the release tag:

1. Set the release version in `VERSION`, `openstrata.toml`,
   `plugins/pointcloud-las/openstrata.plugin.yaml`,
   `plugins/pointcloud-las/CMakeLists.txt`,
   `plugins/pointcloud-laz/openstrata.plugin.yaml`, and
   `plugins/pointcloud-laz/CMakeLists.txt`,
   `plugins/pointcloud-copc/openstrata.plugin.yaml`,
   `plugins/pointcloud-copc/CMakeLists.txt`,
   `plugins/pointcloud-ply/openstrata.plugin.yaml`, and
   `plugins/pointcloud-ply/CMakeLists.txt`.
2. Run `python tools/check_release_metadata.py` to verify that all package
   version declarations match `VERSION`.
3. Update `CHANGELOG.md`, the release record, and any capability or
   compatibility documentation that changed for the release.
4. Run `ost ci validate`, `ost configure`, `ost build`, and `ost test`.
5. Package all plugins with `ost plugin package --workspace --product
   --target cy2026 --profile usd --json`, then inspect the generated product,
   manifest, and SBOM names for the expected version.
6. Commit the complete release preparation change and create `vX.Y.Z` on that
   commit. Push the tag only after the checks pass.

The release workflow validates the tag against `VERSION` and checks out the
tagged commit on every platform. Changes made after tagging are therefore not
included in that release build.

The v0.2.0, v0.2.1, and v0.2.2 records are finalized for their tagged commits
and are not rewritten after publication.
