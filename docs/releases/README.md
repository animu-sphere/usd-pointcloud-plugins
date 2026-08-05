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

1. `VERSION`, `openstrata.toml`, both plugin manifests, both plugin CMake
   projects, the tag, and the finalized changelog version agree;
2. every declared hosted CI cell in `openstrata.ci.yaml` passes;
3. package digests are reproducible for an unchanged build;
4. notices, SBOM/provenance policy, and target metadata are verified —
   including the LGPL-2.1 obligations that apply to `pointcloud-laz`, per
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

## Release procedure

Run these steps from the repository root before creating the release tag:

1. Set the release version in `VERSION`, `openstrata.toml`,
   `plugins/pointcloud-las/openstrata.plugin.yaml`,
   `plugins/pointcloud-las/CMakeLists.txt`,
   `plugins/pointcloud-laz/openstrata.plugin.yaml`, and
   `plugins/pointcloud-laz/CMakeLists.txt`.
2. Run `python tools/check_release_metadata.py` to verify that all package
   version declarations match `VERSION`.
3. Update `CHANGELOG.md`, the release record, and any capability or
   compatibility documentation that changed for the release.
4. Run `ost ci validate`, `ost configure`, `ost build`, and `ost test`.
5. Package both plugins with `ost plugin package --workspace --product
   --target cy2026 --profile usd --json`, then inspect the generated product,
   manifest, and SBOM names for the expected version.
6. Commit the complete release preparation change and create `vX.Y.Z` on that
   commit. Push the tag only after the checks pass.

The release workflow validates the tag against `VERSION` and checks out the
tagged commit on every platform. Changes made after tagging are therefore not
included in that release build.

The v0.2.0, v0.2.1, and v0.2.2 records are finalized for their tagged commits
and are not rewritten after publication.
