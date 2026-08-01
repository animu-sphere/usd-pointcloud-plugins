# ADR-0002: LAZ Codec

## Status

Accepted for the first LAZ reader implementation and shipped in v0.1.0.

## Decision

Use laz-perf as the LAZ decompression backend, isolated behind `usdLaz`.

`usdLaz` exposes project-owned types and a chunk reader contract. The laz-perf
adapter remains private to the library. The OpenUSD plugin depends on `usdLaz`
but never includes laz-perf headers.

## Rationale

- laz-perf is a focused C++ codec rather than a broad format-processing stack.
- It supports incremental decompression, which matches the chunk and bounded
  memory requirements for the LAZ milestone.
- It keeps LAZ optional; LAS and the shared libraries do not acquire a codec
  dependency.
- PDAL is deferred because its broader pipeline and dependency graph are not
  needed for direct LAZ stage loading.
- LASzip remains a fallback if the pinned OpenStrata environment cannot provide
  a compatible laz-perf package.

## Consequences

- The package and version must be pinned before the concrete adapter is added.
- Reader tests can use a project-owned fake decoder and do not require OpenUSD.
- The plugin must preserve the LAS logical model and consume chunks rather than
  materializing the compressed source.
