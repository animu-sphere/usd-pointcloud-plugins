# PLY corpus

This directory contains small, checked-in subsets of point-cloud data.
The exact conformance coverage remains in `../fixtures`; these corpus assets
exercise ASCII and binary PLY vertex decoding with realistic scalar attributes.

Corpus files are test inputs and are not installed with the plugin bundle. Each
dataset directory records its source, license, fixed source revision where
applicable, thinning rule, output count, and SHA-256 in `PROVENANCE.md`.

Regenerate the subsets with `tools/thin_ply_corpus.py` after downloading the
source files described by each provenance file.