# OST usd-http-resolver pre-implementation dogfooding

- Date: 2026-08-16
- Consumer release target: `v0.10.0`
- Resolver repository: `animu-sphere/usd-http-resolver`
- Resolver revision: `2fc935b50000bf7a05eff407187541a6bee77420` (`main`)
- OST version exercised locally: `ost 0.22.2`
- Runtime: `cy2026` / OpenUSD 26.08
- Platform: Windows x86_64
- Target: `cy2026-windows-x86_64-py313-usd`
- Scope: validate the new resolver repository's documented OpenStrata entry
  points before making it a v0.10.0 external integration dependency

This is an engineering dogfooding record, not a resolver interoperability
result. The inspected revision declares itself pre-implementation: it contains
the project skeleton and documentation, but no resolver bundle, HTTP backend,
raw-byte cache, registered tests, or release.

## 1. Result summary

| Check | Result | Evidence |
| --- | --- | --- |
| Resolver source availability | pass | GitHub `main` resolved to `2fc935b50000bf7a05eff407187541a6bee77420` |
| `ost --version` | pass | `ost 0.22.2` |
| `ost build` | pass | Generated the Windows USD target and completed all four workflow steps; CMake reported `ninja: no work to do` |
| `ost ci validate` | expected repository setup failure | `openstrata.ci.yaml` is not present; OST reported `PRECONDITION_FAILED` and suggested `ost ci init` |
| `ost test` | expected repository setup failure | CTest found no tests; OST reported `EXTERNAL_TOOL_FAILED: no tests ran` |

## 2. Commands exercised

```text
git clone --depth 1 https://github.com/animu-sphere/usd-http-resolver.git
ost --version
ost ci validate
ost build
ost test
```

## 3. Interpretation

The successful `ost build` confirms that the current resolver skeleton can
resolve the pinned `cy2026` / `usd` environment and configure its intended
build graph. The other two outcomes reflect intentionally incomplete repository
configuration, rather than an OpenStrata behavior defect:

- `ost ci validate` requires an explicit support matrix. The resolver
  repository should add `openstrata.ci.yaml` when it introduces CI coverage.
- `ost test` correctly fails closed when no `add_test()` entries exist. The
  resolver repository should register its local-backend and first HTTP-backend
  tests with their implementation.

No OpenStrata product change is requested from this observation. The
precondition and no-tests diagnostics are actionable and accurately identify
the repository work that remains.

## 4. v0.10.0 consequence

`usd-pointcloud-plugins` keeps Tier 1 resolver-identity and generated-cache
coverage self-contained. It must not make the external resolver a build-time
dependency or remove its memory-backed test double based on this skeleton.

After `usd-http-resolver` ships its first resolver bundle, backend, and stable
identity metadata, repeat this record as Tier 2 evidence with a reproducible
HTTP fixture. That run must exercise runtime plugin composition, COPC metadata
and range reads, generated-cache miss-to-hit reuse, validation-token
invalidation, and the resolver's `bytes fetched / source size` baseline.

## Related documents

- [Resolver-backed source contract](../../architecture/RESOLVER_SOURCE.md)
- [Infrastructure maturity roadmap](../../roadmap/infrastructure-maturity.md)
- [OST report index](README.md)