# Module README contract

Every directory under `libs/` and `plugins/` contains a `README.md`. That
README is part of the module contract, not optional supplementary
documentation: it is where the module states what it owns, what it refuses to
own, and what a caller may rely on.

This is invariant 13 of the
[workspace contract](../architecture/WORKSPACE.md).

## Ownership

A code change that modifies a module contract updates that module's `README.md`
in the same pull request. Examples:

- adding or removing a public API;
- changing a file-format argument;
- changing supported source versions, point formats, or attributes;
- changing coordinate handling;
- changing ownership or lifetime rules for returned buffers;
- adding or removing a runtime dependency;
- changing license obligations.

A reviewer treats a contract change without a README change as incomplete.

## Required sections: library README

```text
# Module name

## Purpose
## Responsibilities
## Non-responsibilities
## Public API
## Dependencies
## Data flow
## Error and diagnostic behavior
## Threading and ownership
## Build and test
## Known limitations
## Planned work
```

A library README must additionally:

- state whether OpenUSD is required;
- state whether the module can be tested without an OpenUSD runtime;
- identify ownership and lifetime rules for returned buffers;
- document coordinate-space assumptions;
- link to the relevant architecture contracts;
- include a minimal usage example where practical.

## Required sections: plugin README

```text
# Plugin name

## Purpose
## Supported file extensions
## Supported source versions and point formats
## FileFormat arguments
## Authored OpenUSD result
## Plugin discovery and installation
## Build and test
## Runtime dependencies
## Licensing
## Known limitations
## Compatibility
```

A plugin README must clearly distinguish:

- reader support;
- authoring-library support;
- features connected to direct FileFormat reads;
- features available only through the lower-level APIs.

Collapsing those four into a single "supported" column is the specific failure
this contract exists to prevent: the authoring library has supported tiled,
payload-backed output since before any FileFormat argument could reach it, and
a README that hides the difference misrepresents what opening a `.las` file
actually does.

## Relationship to the shared documentation

A module README describes that module. It does not restate a shared contract;
it links to it. The normative sources are:

| Topic | Document |
| --- | --- |
| Structure, identities, dependency directions | [WORKSPACE.md](../architecture/WORKSPACE.md) |
| Supported source data and authored USD | [CAPABILITY_MATRIX.md](../reference/CAPABILITY_MATRIX.md) |
| Tiling and LOD | [LOD.md](../architecture/LOD.md) |
| Read options and streaming | [POINT_READER.md](../architecture/POINT_READER.md) |
| File-format arguments | [FILE_FORMAT_ARGUMENTS.md](../architecture/FILE_FORMAT_ARGUMENTS.md) |
| Adapter thinness | [PLUGIN_ADAPTER.md](../architecture/PLUGIN_ADAPTER.md) |
| Diagnostics | [DIAGNOSTICS.md](../architecture/DIAGNOSTICS.md) |
| Licensing and redistribution | [DISTRIBUTION.md](../guides/DISTRIBUTION.md) |

When a module README and one of those documents disagree, the shared document
wins and the README is the bug.

## Status language

Use status words that separate capability from reachability:

```text
implemented                   in this module, tested
implemented, not connected    exists in a library, no argument reaches it
planned                       has a contract, no implementation
not planned                   explicitly out of scope
```

"Not implemented" without qualification is only correct when no code in the
repository performs the behavior.
