# RFC 8259 conformance — measured verdict

**Measured 2026-09-14, refreshed 2026-09-16** against the vendored corpus in
`third_party/json_test_suite` (nst/JSONTestSuite, MIT, (c) 2016 Nicolas Seriot). Run it:

```bash
cmake --build build --target run_conformance      # verdict; exits non-zero on disagreement
cmake --build build --target conformance_report   # + the i_ list (decisions, not bugs)
```

## Where things stand

| class | meaning | result (2026-09-16) | result (2026-09-14) |
|---|---|---|---|
| `y_` — must accept | 95 files | **95/95 OK** | 95/95 OK |
| `n_` — must reject | 188 files | **146/188 — 42 disagreements** | 144/188 |
| `i_` — suite abstains | 35 files | 30 accepted, 5 rejected | 31 / 4 |
| crashes (never acceptable) | — | **0** | 2 |

The two crash files are now clean rejections, and `i_structure_500_nested_arrays.json`
moved from "accepted" to "rejected" because 500 levels is past the new nesting limit —
a policy decision, not a conformance loss (see the README's "Resource Limits").

## Finding 1 — a valid 60 KB document crashed the parser (FIXED 2026-09-16)

30,000 nested `[` closed by 30,000 `]` is **valid JSON** and segfaults:

```bash
python3 -c "print(chr(91)*30000 + chr(93)*30000, end='')" > /tmp/deep.json
./jsom format /tmp/deep.json        # Segmentation fault
```

ASan names the recursion: `FastParser::parse_array()` <-> `FastParser::parse_value()`,
123 frames deep and climbing until the stack is gone. There is **no nesting-depth
limit**, so this is a remote DoS for anything parsing untrusted JSON: ~60 KB in,
process dead. Measured: 20,000 levels accepted, 28,000 rejected (EOF), **30,000
segfault** — arrays, objects and mixed nesting alike.

### Resolution

A depth bound now exists — one integer comparison per container, checked on the way
*in*, because a stack overflow cannot be caught: `JsonParseOptions::max_depth`
(default `limits::MAX_NESTING_DEPTH` = 256) for parsing, and the same bound for every
traversal (serialize, compare, path listing, formatting). Deeper input, including
`[`*100000 with no closers, is rejected with
`std::runtime_error: Maximum nesting depth exceeded (limit 256)`. The limit was chosen
from measured stack cost per level (~0.3–0.6 KB, worst case a debug build) so the
deepest legal document fits a 1 MB thread stack; `tests/test_nesting_limits.cpp` proves
that on a real 1 MB thread, and pins the historical crash shapes as rejections.

The boundary that matters is not a number of brackets but the caller's stack: the same
document parses at 20,000 levels on an 8 MB main thread and dies at ~3,000 levels on a
1 MB worker thread, and the threshold moves with the compiler and optimisation level.
That is exactly why the limit is explicit and configurable rather than left to luck.

Why the fuzzer never found it: fuzz inputs are capped at 4 KB, and 4 KB of `[` is
only ~4,000 levels — nowhere near the ~30,000 needed to reach the old cliff. The
suite's hand-written pathological case reaches it immediately. RFC 8259 imposes no
depth limit, which is why the suite files this under `n_`: rejecting it with a proper
error is conformant, and so is parsing it iteratively. Dying is not.

The arithmetic works the other way round now, which is a quiet bonus of a bound: the
limit (256) is reachable from a **256-byte** fuzz input, so ordinary fuzzing exercises
the guard. `fuzz/seeds/deep_nesting.json` (400 levels, 800 bytes) is checked in for
exactly that.

## Finding 2 — malformed numbers and escapes are accepted (44 files)

- **25 malformed numbers**: `-01`, `1.0.`, `-2.`, `0.1.2`, `1eE2`, `0e+`, `2.e+3`,
  `9.e+`, `01`, and `[-]`. Numbers are validated *lazily* (`LazyNumber` preserves
  the original text), so the number grammar is not enforced during the scan.
- **15 malformed strings/escapes**: `\u` with too few hex digits, `\x`, `\0`,
  lone-surrogate combinations, `\U`.
- **3 structures**, including whitespace-formfeed.

Whether to validate during the scan or behind a `JsonParseOptions` flag is a
policy call, and it is deliberately left open here.

## Finding 3 — the round-trip oracle fires (found by the fuzzer, not the suite)

`tests/fuzzer.cpp` now asserts `parse(to_json(doc)) == doc`. It aborted within
60 seconds on a 150-byte input, preserved at
`fuzz/regressions/round-trip-malformed-escape.json`:

```
in:  ...unicode: \u00e\0\0 9\u4e2d\u6587...
out: ...unicode: \\u00e\u0000\u00009\\u4e2d\\u6587...   (backslashes escaped on output)
```

An input carrying a *malformed* escape is accepted, its backslash is escaped on
output, so the document changes identity across a single round trip. That
contradicts the stated intent of the default mode — "preserves \uXXXX as literal
strings for **round-trip fidelity**" — independently of RFC 8259. Same root cause
as the escape group in Finding 2.

## Finding 4 — the streaming path cannot parse empty containers (found 2026-09-16)

Pre-existing and unrelated to the depth work, but worth recording because it was found
while testing the streaming path's depth contract: `parse_document_streaming()` throws
on every empty container.

```text
[]            -> Parse error at position 2 (path: /0): Unexpected character
[[]]          -> Parse error at position 3 (path: /0/0): Unexpected character
{"a":{}}      -> Parse error at position 7 (path: /a): Unexpected character
[1] / [[1]]   -> fine
```

The event stream itself is correct — the fast parser accepts all of these, and the
enter/value events for the streaming path carry the right paths — so this is in
`StreamingParser`'s state machine, on the path that the fuzzer has never reached
(`tests/fuzzer.cpp` exercises `parse_document` only, and the streaming parser is
described in the code as "legacy, for compatibility/debugging"). It is the same class
of gap as Finding 2: an oracle that never sees a shape cannot report it.

## Next steps

1. Decide the number/escape policy (Finding 2); that also silences Finding 3.
2. Add a nesting-depth limit, or make the parser iterative (Finding 1).
3. Then consider making `run_conformance` part of the gate set. Today it is an
   explicit target on purpose, so the verdict is visible without red-lighting
   the default build.
