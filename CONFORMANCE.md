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

## Finding 2 — malformed numbers and escapes are accepted (42 files; numbers now opt-in fixable)

- **25 malformed numbers**: `-01`, `1.0.`, `-2.`, `0.1.2`, `1eE2`, `0e+`, `2.e+3`,
  `9.e+`, `01`, and `[-]`. Numbers are validated *lazily* (`LazyNumber` preserves the
  original text), so the number grammar is not enforced during the scan.
- **15 malformed strings/escapes**: `\u` with too few hex digits, `\x`, `\0`,
  lone-surrogate combinations, `\U`, unescaped control characters, invalid UTF-8.
- **1 structure**: whitespace-formfeed.
- Plus `n_array_just_minus` — `[-]`, which is a number-grammar problem too.

### Numbers: a switch, off by default (2026-09-16)

`JsonParseOptions::validate_numbers` (and the `ParsePresets::Strict` preset) enforces
the RFC 8259 §6 number grammar during the scan:

```bash
./build/jsom_conformance                # numbers: lazy (default)   -> n_ 146/188
./build/jsom_conformance --strict-numbers  # numbers: VALIDATED    -> n_ 172/188
```

26 of the 42 disagreements fall to that one flag, with `y_` still **95/95** (it rejects
nothing valid). It defaults to **off**, which is a policy decision rather than an
oversight: RFC 8259 §9 permits a parser to accept non-JSON forms, so accepting `-01` is
defensible *as a documented extension* — the point of the switch is that it is now a
decision instead of an accident. The reason for the default is cost, measured (see
OPTIMIZATIONS.md): +6% parsing a document of short numbers, +12% with 17-digit numbers,
+0.5% on a realistic mixed payload.

Validation does not force conversion: `LazyNumber` still stores the original text, so
round trips stay byte-exact (pinned by `NumberValidationTest.StrictModeKeepsTheOriginalText`).

The remaining 16 disagreements are the **escape/UTF-8 group** (15) and the formfeed
whitespace case, which are a separate decision: escapes are entangled with the
documented round-trip-fidelity mode (`convert_unicode_escapes = false` preserves
`\uXXXX` literally, which is exactly why a malformed escape survives the scan).

**Reference point for the policy**: on the same corpus, nlohmann/json 3.11.3 scores
`y_` 95/95, `n_` **187/188** (one disagreement), `i_` 7 accepted / 28 rejected. Strict
number validation closes JSOM's number gap; the escape gap and the `i_` difference are
where the two libraries still differ.

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

1. **Escapes are the remaining policy call** (Finding 2's other half, 15 files, and the
   root cause of Finding 3's round-trip mismatch). Numbers are done as a switch
   (`validate_numbers`, default off) and something equivalent is needed here — the
   wrinkle is that fidelity mode deliberately preserves `\uXXXX` as text, so the
   decision is what a *malformed* escape means in a mode whose promise is "no character
   loss". That decision also un-reds the `fuzz_quick` gate.
2. ~~Add a nesting-depth limit, or make the parser iterative~~ — done 2026-09-16: the
   limit is `limits::MAX_NESTING_DEPTH` (256, per-parse via `max_depth`), enforced on
   every traversal; crashes 2 → 0. An iterative rewrite would still need the limit for
   the traversals, so it stayed a constant.
3. Consider making `run_conformance` part of the gate set. Today it is an explicit
   target on purpose, so the verdict is visible without red-lighting the default build.
   It now also takes `--strict-numbers` so the strict verdict can be checked in CI once
   the escape policy is settled.
