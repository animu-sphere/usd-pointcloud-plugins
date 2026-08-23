# Resolver-backed read baseline

Recorded Tier 2 numbers for reading a COPC asset through an external OpenUSD
resolver. The contract these measure is
[RESOLVER_SOURCE.md](../architecture/RESOLVER_SOURCE.md); Tier 1, the
repository-local gate that runs with no external resolver, is a different thing
and lives in the CI matrix.

Last recorded: 2026-08-23, for `v0.10.0`.

The machine-readable record is
[resolver-tier2-record.json](resolver-tier2-record.json). This file explains it;
the JSON is the evidence.

## What produced these numbers

| Component | Version | Role |
| --- | --- | --- |
| `usd-pointcloud-plugins` | `v0.10.0` | `pointcloud-copc` FileFormat, generated-cache decisions |
| [`usd-http-resolver`](https://github.com/animu-sphere/usd-http-resolver) | `v0.4.0` | `ArResolver` for `http`/`https`, range reads, identity through `ArAssetInfo` |
| `tools/tier2_fixture_server.py` | in-tree | loopback origin that honours `Range` and logs every request |
| Autzen classified COPC | PDAL `data` distribution | 81,123,042 bytes, 10,653,336 points, SHA-256 `db2d56cd…e25a27fa` |
| OpenStrata runtime | `cy2026` / `usd` (OpenUSD 26.08) | host |

Neither repository is in the other's build graph. They compose at runtime
through `PXR_PLUGINPATH_NAME`, which is the whole of the integration.

## Reproducing

```powershell
# Fetch the fixture once; see docs/roadmap/streaming-and-tiling.md for provenance.
Invoke-WebRequest `
  -Uri https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz `
  -OutFile build/real-data-source/autzen-classified.copc.laz
Copy-Item build/real-data-source/autzen-classified.copc.laz build/tier2/autzen.copc

Invoke-Expression (& ost env cy2026 --profile usd --shell powershell | Out-String)
python tools/tier2_resolver_integration.py `
  --fixture build/tier2/autzen.copc `
  --resolver-resources <usd-http-resolver>/plugins/http-resolver/plugin/resources/httpResolver `
  --copc-resources plugins/pointcloud-copc/plugin/resources/pointcloud-copc `
  --output build/tier2/record.json
```

The URL path must end in `.copc`, because OpenUSD selects the FileFormat by
extension. Each scenario runs in a fresh interpreter against a fresh origin, so
no in-process resolver state and no request log carries between rows.

## Recorded results

`selectivity` is `bytes fetched / source size`. `identity` is the class this
repository derives from what the resolver published, and `codes` are the
generated-cache decision codes OpenUSD reported.

| Scenario | Revision | Validator | Identity | Codes | Requests | Bytes fetched | Selectivity | Points |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: |
| full, local file | — | — | — | — | — | — | — | 10,653,336 |
| metadata only | A | strong | stable | — | 3 | 120,546 | 0.001486 | — |
| full read | A | strong | stable | COPC010 | 277 | 81,123,042 | 1.000000 | 10,653,336 |
| metadata only | B | strong | stable | — | 3 | 120,546 | 0.001486 | — |
| full read | B | strong | stable | COPC010 | 277 | 81,123,042 | 1.000000 | 10,653,336 |
| metadata only | W | weak | unstable | — | 3 | 120,546 | 0.001486 | — |
| full read | W | weak | unstable | COPC009 | 277 | 81,123,042 | 1.000000 | 10,653,336 |

Revisions A, B, and W serve identical bytes under identical identifiers and
differ only in the validator the origin publishes. That is deliberate: it
separates *what was resolved* from *which revision of it*, which is the
distinction the whole identity contract rests on.

### Output equivalence

Every row that authored points produced 10,653,336 points and the same SHA-256
over the authored positions, `c6cb6109…76c1d2f6`, including the local-file row.
A resolver-backed read and a local read are the same authored asset.

### Bounded read

A metadata-only open costs three requests and 120,546 bytes: 0.15% of the asset.
The COPC header and its info VLR are what a metadata open needs, and that is
what crosses the wire. This is the number that justifies range access existing.

### Full read

A full read fetches the whole asset, in 277 requests of which 276 are ranges,
and selectivity is exactly 1.0. That is the correct answer, not a defect: a read
that authors every point needs every point. It is recorded because a range
reader that loses to a plain download on the full-read case has a coalescing
policy that the bounded number would be hiding.

### Identity and the conservative fallback

The strong-validator rows classify as `stable` and report `COPC010`, the
reuse-permitted category. The weak-validator row classifies as `unstable` and
reports `COPC009`, reuse disabled — and still authors identical output, because
a disabled cache changes what is reused, never what is read.

That row is the fallback proven against a released external resolver rather than
against a test double: `usd-http-resolver` publishes a token in
`ArAssetInfo::version` only for a validator strong enough to prove two responses
are the same bytes, and this repository enables reuse only when a token is
present. Neither side negotiates; one value crosses the boundary.

Revisions A and B carry different validation tokens for one identifier, so they
derive different generated-cache identities. Equal identifiers never imply equal
content, and this record is the demonstration.

## What this baseline does not measure

- **A generated-cache hit ratio for COPC.** Generated entries are published by
  `usd-pointcloud-convert`, which accepts `.las` and `.laz` local inputs only, so
  no COPC read — local or resolver-backed — has an entry to hit in a normal
  workflow. What is verified here is the decision: which identity permits reuse,
  and which diagnostic explains it. Reuse, invalidation on a changed token,
  incomplete-entry recovery, and corrupted-entry recovery are covered by Tier 1
  against committed entries.
- **Raw byte-range cache behavior.** That cache belongs to the resolver, and its
  hit ratios are recorded in that repository's own baseline.
- **Wide-area network behavior.** The origin is loopback. These are protocol and
  selectivity numbers, not latency numbers.
