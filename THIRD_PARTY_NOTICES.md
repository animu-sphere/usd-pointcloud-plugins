# Third-Party Notices

This file records third-party software and test data distributed with or
referenced by this repository. The Apache-2.0 license in the repository root
applies to the original project code only; it does not replace the licenses
listed below.

## laz-perf 2.0.0

The LAZ and COPC readers incorporate the upstream `laz-perf` source from:

- Upstream: https://github.com/hobu/laz-perf
- Version: 2.0.0
- Commit: `2e3c316248fa534cdeba1b47b2e9fe1a0ecf5dca`
- Local source: `third_party/laz-perf/cpp/lazperf`

`laz-perf` is distributed under the GNU Lesser General Public License,
version 2.1 (LGPL-2.1). The complete license text is retained at
`third_party/laz-perf/COPYING`. The vendoring scope and omitted upstream
components are described in `third_party/laz-perf/VENDORING.md`.

The laz-perf sources are compiled into the static library `usdLaz`, which is
linked into the `pointcloud-laz` and `pointcloud-copc` plugin binaries. The
`pointcloud-las` plugin contains no laz-perf code.

When distributing binaries that include the LAZ or COPC adapter, preserve this
notice and the LGPL-2.1 terms. The project does not relicense the upstream
`laz-perf` source under Apache-2.0. The concrete obligations for a binary
release are recorded in [docs/guides/DISTRIBUTION.md](docs/guides/DISTRIBUTION.md).

## OpenUSD

OpenUSD is a required runtime dependency for the optional USD authoring and
FileFormat Plugin targets. It is not vendored by this repository. Follow the
license and notice requirements of the OpenUSD distribution used to build and
ship a plugin bundle.

## Test data

The LAS and LAZ corpus files under `plugins/*/tests/corpus` are test fixtures,
not project code. Each corpus and dataset directory contains provenance and
license information for its source data. Those terms apply to the fixtures
and must be preserved when redistributing them.