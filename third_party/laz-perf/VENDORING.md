# Vendored laz-perf

This directory contains the upstream laz-perf source used privately by
`libs/usd-laz`.

## Source

- Upstream: <https://github.com/hobu/laz-perf>
- Tag: `2.0.0`
- Commit: `2e3c316248fa534cdeba1b47b2e9fe1a0ecf5dca`
- License: GNU LGPL 2.1 (`COPYING`)

The upstream runtime source under `cpp/lazperf` is kept unmodified. Upstream
tests, GoogleTest, examples, benchmarks, tools, Python bindings, and Emscripten
assets are omitted. The owning CMake target compiles only the library sources
under `cpp/lazperf` and does not expose laz-perf headers in the public `usdLaz`
include path.