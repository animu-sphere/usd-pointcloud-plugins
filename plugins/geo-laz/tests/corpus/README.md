# LAZ corpus

This directory contains small, checked-in subsets of real LAZ data. Synthetic
fixtures in `../fixtures` remain the exact conformance coverage; corpus assets
exercise the reader against public survey data with structural checks only.

Each asset records its source, license, thinning rule, output count, and SHA-256
in `PROVENANCE.md`. Corpus files are test inputs and are not installed with the
plugin bundle.

The checked-in real-data subset is from Virtual Shizuoka and is distributed
under the dataset's CC BY 4.0/ODbL dual license. It is not public-domain data.

The USGS 3DEP subset is public-domain data from the U.S. Government.

The target coverage, one fixture per LAS version and point format plus the
negative corpus, is defined in the
[development policy](../../../../docs/development-policy.md). New fixtures are
preferred from CC0 or public-domain sources so that redistribution stays
unencumbered.