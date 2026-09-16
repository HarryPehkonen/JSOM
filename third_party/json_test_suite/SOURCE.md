# json_test_suite (vendored)

Upstream: https://github.com/nst/JSONTestSuite (nst/JSONTestSuite)
Commit:   1ef36fa
Licence:  MIT, (c) 2016 Nicolas Seriot — see LICENSE in this directory.
Vendored: 2026-09-14, test_parsing/ + test_transform/ + LICENSE only.

File-name convention (from the upstream README):

- `y_*` — content **must be accepted** by parsers
- `n_*` — content **must be rejected** by parsers
- `i_*` — parsers are free to accept or reject; the suite **has no opinion**
- `test_transform/` — weird structures parsers may understand differently (informational)

The `i_` class is the interesting one: it lists the places where the spec leaves a
choice, so those are design decisions, not bugs. Run `jsom_conformance --list` to
see which side JSOM took on each.

Consumed by `tools/conformance_runner.cpp`; run it with
`cmake --build build --target run_conformance`.
