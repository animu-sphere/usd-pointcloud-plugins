# Binary Distribution and Licensing

This document fixes how plugin binaries may be distributed. It complements
[THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md), which records what is
included, and section 12 of the [design policy](../design/DESIGN_POLICY.md).

It describes engineering obligations, not legal advice. A qualified reviewer
confirms the interpretation before the first public binary release.

## Licenses In Play

| Component | License | Distributed as |
| --- | --- | --- |
| Project code (`libs/`, `plugins/`) | Apache-2.0 | Source and binaries |
| `laz-perf 2.0.0` | LGPL-2.1 | Vendored source, compiled into `geospatial-laz` |
| OpenUSD | Apache-2.0 (per distribution) | Not vendored; runtime dependency |
| Test corpus | Per-dataset terms | Not shipped in plugin bundles |

`geospatial-las` contains no laz-perf code. Only `geospatial-laz` is affected by LGPL-2.1.

## Link Model

The build compiles the upstream sources under `third_party/laz-perf/cpp/lazperf`
directly into the static library `usdLaz`, which is then linked into the
`geospatial-laz` plugin shared library.

```text
third_party/laz-perf/cpp/lazperf/*.cpp
  -> usdLaz (STATIC)
    -> geospatial-laz plugin (SHARED)
```

Consequences:

- The shipped `geospatial-laz` binary is a combined work that contains LGPL-2.1 object
  code, so LGPL-2.1 section 6 applies to its distribution.
- laz-perf headers are not exposed through the public `usdLaz` include path,
  and no laz-perf type appears in a public API.
- The upstream source under `cpp/lazperf` is unmodified. Omitted upstream
  components are listed in
  [VENDORING.md](../../third_party/laz-perf/VENDORING.md).

If the vendored source is ever modified, the change must be marked in the
files, described in `VENDORING.md`, and reflected in
`THIRD_PARTY_NOTICES.md`.

## Compliance Requirements

Every distribution that includes `geospatial-laz` binaries provides:

1. The complete LGPL-2.1 text (`third_party/laz-perf/COPYING`).
2. `LICENSE` and `NOTICE` for the project code.
3. `THIRD_PARTY_NOTICES.md`, naming laz-perf, its version, its commit, and its
   license.
4. A prominent statement that the plugin uses laz-perf and that laz-perf is
   covered by LGPL-2.1.
5. The complete corresponding source for laz-perf, matching the exact version
   built.
6. A means for the recipient to relink `geospatial-laz` against a modified laz-perf.

Requirement 6 is what static linking adds. It is satisfied by publishing, in
the same release, the complete source archive of this repository together with
build instructions that reproduce the shipped binary, so a recipient can
rebuild `geospatial-laz` with their own laz-perf. Shipping the intermediate object
files or the static `usdLaz` archive is the fallback if a build from source
ever stops being reproducible.

Switching `geospatial-laz` to link laz-perf as a separate shared library would move
the obligation from section 6(a) to the simpler shared-library case. That
change is a candidate but is not in effect today, and this document must be
updated before any statement to the contrary is published.

## Release Artifact Layout

A published release contains, per platform:

| Artifact | Purpose |
| --- | --- |
| Plugin product bundle | `geospatial-las` and `geospatial-laz` libraries and `plugInfo.json` |
| `*.manifest.json` | Bundle manifest emitted by `ost plugin package` |
| `*.sbom.spdx.json` | SBOM for the packaged bundle |
| Source archive | Corresponding source for the tag, including laz-perf |
| `LICENSE`, `NOTICE` | Project license and attribution |
| `THIRD_PARTY_NOTICES.md` | Third-party components and their terms |
| `COPYING` (laz-perf) | LGPL-2.1 text |
| `CAPABILITY_MATRIX.md` | The format and attribute matrix for the release |
| `OPENUSD.md` | OpenUSD and OpenStrata compatibility statement |
| `INSTALL.md` | Plugin discovery and installation instructions |
| `SHA256SUMS` | SHA-256 for every artifact |

The release workflow currently publishes the plugin products, manifests, SBOMs,
the source archive, release notes, and `SHA256SUMS`. Staging the license,
notice, capability, compatibility, and installation documents into the release
assets is outstanding work and blocks the first public binary release.

## Verification Before Publishing

- [ ] The tag matches `VERSION`.
- [ ] `THIRD_PARTY_NOTICES.md` matches the vendored laz-perf commit.
- [ ] `VENDORING.md` matches the vendored tree, including any modification.
- [ ] LGPL-2.1 text is present in the assets.
- [ ] The source archive builds `geospatial-laz` on every published platform.
- [ ] Checksums verify.
- [ ] A qualified reviewer has confirmed the licensing statement.
