# RFC 8259 conformance — measured verdict

**Measured 2026-09-14** against the vendored corpus in `third_party/json_test_suite`
(nst/JSONTestSuite, MIT, (c) 2016 Nicolas Seriot). Run it:

```bash
cmake --build build --target run_conformance      # verdict; exits non-zero on disagreement
cmake --build build --target conformance_report   # + the i_ list (decisions, not bugs)
```

## Where things stand

| class | meaning | result |
|---|---|---|
| `y_` — must accept | 95 files | **95/95 OK** |
| `n_` — must reject | 188 files | **144/188 — 44 disagreements** |
| `i_` — suite abstains | 35 files | 31 accepted, 4 rejected |
| crashes (never acceptable) | — | **2** |

## Finding 1 — a valid 60 KB document crashes the parser (high severity)

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

Why the fuzzer never found it: fuzz inputs are capped at 4 KB, and 4 KB of `[` is
only ~4,000 levels — nowhere near the ~30,000 needed. The suite's hand-written
pathological case reaches it immediately. RFC 8259 imposes no depth limit, which
is why the suite files this under `n_`: rejecting it with a proper error is
conformant, and so is parsing it iteratively. Dying is not.

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

## Next steps

1. Decide the number/escape policy (Finding 2); that also silences Finding 3.
2. Add a nesting-depth limit, or make the parser iterative (Finding 1).
3. Then consider making `run_conformance` part of the gate set. Today it is an
   explicit target on purpose, so the verdict is visible without red-lighting
   the default build.
