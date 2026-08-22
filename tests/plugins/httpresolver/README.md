# httpresolver (test double)

`httpresolver` is **not a product bundle and not an HTTP client**. It is an
OpenUSD `ArResolver` test double used by the COPC integration tests. It exposes
`http://memory.copc` and `https://memory.copc` and serves the bytes from the
file named by `USDGEOCOPC_TEST_ASSET` as an in-memory `ArAsset`.

Do not deploy it as a transport. It performs no network I/O and implements no
range requests, redirects, retries, timeouts, authentication, or caching. Those
belong to a real resolver implementation in a separate repository; a production
resolver can provide the same `ArAsset` surface with efficient range reads
without changing this plugin or `usdCopc`.

## Status

The fixture lives under `tests/plugins/httpresolver/` so that Tier 1 resolver
contract tests remain reproducible without an external repository. It is built
only as a dependency of the COPC integration tests and has no OpenStrata bundle
manifest or install rule, so it is excluded from product discovery, packaging,
the plugin matrix, and release metadata.

Tier 2 interoperability uses
[`usd-http-resolver`](https://github.com/animu-sphere/usd-http-resolver) as
runtime composition and does not link this repository to it.

See the
[resolver-backed source contract](../../../docs/architecture/RESOLVER_SOURCE.md)
and the [workspace contract](../../../docs/architecture/WORKSPACE.md).
