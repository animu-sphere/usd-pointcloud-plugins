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

`v0.10.0` resolves this bundle's disposition: it is removed once equivalent
external integration coverage exists, or relocated to an explicitly test-only
path such as `tests/plugins/httpresolver/`. Until then it must not be presented
as equivalent to a production point-cloud bundle.

See the
[resolver-backed source contract](../../docs/architecture/RESOLVER_SOURCE.md)
and the [workspace contract](../../docs/architecture/WORKSPACE.md).
