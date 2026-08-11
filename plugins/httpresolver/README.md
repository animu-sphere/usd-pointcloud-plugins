# httpresolver

`httpresolver` is an OpenUSD `ArResolver` bundle used by the COPC integration
tests. It exposes `http://memory.copc` and `https://memory.copc` and serves the
bytes from the file named by `USDGEOCOPC_TEST_ASSET` as an in-memory
`ArAsset`.

This is a resolver test double, not an HTTP client. It deliberately keeps
network transport, authentication, retries, and network caching outside this
repository. A production HTTP resolver can provide the same `ArAsset` surface
with efficient range reads without changing the `usdCopc` reader.