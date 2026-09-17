# RFC 8259 conformance — measured verdict

**Measured 2026-09-14, refreshed 2026-09-16** against the vendored corpus in
`third_party/json_test_suite` (nst/JSONTestSuite, MIT, (c) 2016 Nicolas Seriot). Run it:

```bash
cmake --build build --target run_conformance      # verdict; exits non-zero on disagreement
cmake --build build --target conformance_report   # + the i_ list (decisions, not bugs)
```

## Where things stand

| class | meaning | default (lazy numbers) | `--validation=numbers` |
|---|---|---|---|
| `y_` — must accept | 95 files | **95/95 OK** | 95/95 OK |
| `n_` — must reject | 188 files | **162/188** | **188/188** |
| `i_` — suite abstains | 35 files | 30 accepted, 5 rejected | same |
| crashes (never acceptable) | — | **0** | 0 |

History: `y_` 95/95 and `n_` 144/188 on 2026-09-14 (with 2 crashes), 146/188 after the
depth limit, 162/188 once the lexical rules became unconditional, 188/188 with the
number grammar (measured 2026-09-16).

`i_structure_500_nested_arrays.json` moved from "accepted" to "rejected" because 500
levels is past the nesting limit — a policy decision, not a conformance loss (README
"Resource Limits"). The suite abstains on `i_` either way.

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

## Finding 2 — malformed numbers and escapes were accepted (FIXED 2026-09-16)

The 42 disagreements were 26 numbers (`-01`, `1.0.`, `2.e+3`, `0e+`, `[-]`), 15
malformed escapes/strings/codepoints and 1 whitespace case. Both halves are fixed, with
different defaults on purpose:

**Escapes, control characters, whitespace — enforced ALWAYS, not a setting.** §7 requires
control characters to be escaped inside strings and defines the escape set exactly; §2
defines whitespace as space/tab/LF/CR. Four rules, enforced regardless of options:

1. a raw control character (U+0000..U+001F) inside a string → `Unescaped control character in string`
2. `\u` without exactly four hex digits → `Invalid hex digit in unicode escape`
3. any escape outside `" \ / b f n r t u` → `Invalid escape sequence: \U`
4. formfeed/vertical tab as whitespace → rejected (also fixes a locale bug: `std::isspace()` is locale-dependent)

Rule 3 also removes the old *backslash-dropping*: that path used to append the character
and lose its backslash, silently altering the document. It now rejects, so no accepted
document is ever altered — there is nothing left to preserve, and
`LexicalConformanceTest.NothingIsDroppedFromAnAcceptedDocument` pins it.

**Numbers — opt-in, because of measured cost.** `JsonParseOptions::validate_numbers`
(or `ParsePresets::Validate`, or `--validation=numbers`), default **off**: RFC 8259 §9
permits accepting non-JSON forms, so the lazy default is a documented extension rather
than an accident. Cost: +6% parsing short numbers, +12% for 17-digit numbers, +0.5% on
a realistic payload (OPTIMIZATIONS.md). Validation does not force conversion —
`LazyNumber` still keeps the original text, so `1.500` round-trips byte-exact.

Cost of the always-on lexical rules: **+1.5% on string-heavy parsing**, everything else
within noise (OPTIMIZATIONS.md, "Lexical rules").

### Numbers: the opt-in switch (2026-09-16)

```bash
./build/jsom_conformance                          # numbers lazy (default) -> n_ 162/188
./build/jsom_conformance --validation=numbers     # number grammar on      -> n_ 188/188
./jsom validate --validation=numbers file.json    # the same switch on the CLI
```

`ParsePresets::Validate` is the API spelling. The measured cost is above; the reason the
default is off is speed, and RFC 8259 §9's permission to accept non-JSON forms is what
makes that defensible as a documented extension.

**Reference point for the policy**: on the same corpus, nlohmann/json 3.11.3 scores
`y_` 95/95, `n_` **187/188** (one disagreement, a NUL after digits), `i_` 7 accepted /
28 rejected. With the number grammar on, JSOM and nlohmann differ only on that one file
and on the `i_` list (where the suite has no opinion).

## Finding 3 — the round-trip oracle fired (FIXED 2026-09-16)

`tests/fuzzer.cpp` asserts `parse(to_json(doc)) == doc`. It aborted within 60 seconds on
a 150-byte input, preserved at `fuzz/regressions/round-trip-malformed-escape.json`.

**The real cause turned out to be narrower than "malformed escapes", and it is worth
recording precisely**: the input contained *raw NUL bytes* inside a string. Fidelity mode
stores a raw control byte as itself, the serializer writes it back as the six-character
text `\u0000`, and the fidelity parser reads that text as *text* rather than as the
character — so the document changed identity. Measured minimal cases:

| default mode, document containing | round trip |
|---|---|
| a raw NUL inside a string | **mismatch** |
| a raw tab or raw newline | stable |
| escaped `\u0000` | stable |
| a valid `\uXXXX` | stable |
| a malformed escape (`\U0041`) | stable (was: backslash dropped — now rejected) |

Rule 1 makes the raw-control-character case a rejection, so the mismatch is no longer
reachable through the parser; the oracle now holds for every accepted input, and the
`fuzz_quick` gate is green (252,327 runs, no artifacts, 2026-09-16). The fuzz target
drives **both** configurations on every input — the default (which ships) and
`--validation=numbers` — so the strict rejection paths are fuzzed too.

Documented limitation that remains, and why it is not worth fixing: in fidelity mode,
serializing a raw control character produces `\uXXXX` text that fidelity parsing reads
back as text. Only reachable for input that is invalid JSON (now rejected); making it
re-readable would mean emitting raw bytes (invalid JSON) or decoding `\uXXXX` (the
`convert_unicode_escapes` mode, where it already round-trips).

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

1. **The judged classes are clean**: `y_` 95/95 and `n_` 188/188 with
   `--validation=numbers`. The lexical rules are unconditional, so the default mode
   already reaches 162/188 — nothing here is waiting on a decision.
2. **The streaming path is the remaining known defect** (Finding 4): `parse_document_streaming()`
   rejects `[]` and `{"a":{}}`. Pre-existing, on the legacy path, and the one subsystem
   with no fuzz coverage of its own.
3. Consider making `run_conformance` part of the gate set. It accepts
   `--validation=numbers` so CI can assert the strict verdict, and the lexical rules make
   the default verdict meaningful too (162/188 rather than 146/188).
