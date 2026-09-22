# RFC 8259 conformance — measured verdict

**Measured 2026-09-20** against the vendored corpus in `third_party/json_test_suite`
(nst/JSONTestSuite, MIT, (c) 2016 Nicolas Seriot). Run it:

```bash
cmake --build build --target run_conformance      # verdict; exits non-zero on disagreement
cmake --build build --target conformance_report   # + the i_ list (decisions, not bugs)
```

## Where things stand

| class | meaning | default (§6 enforced) | `--validation=loose` |
|---|---|---|---|
| `y_` — must accept | 95 files | **95/95 OK** | 95/95 OK |
| `n_` — must reject | 188 files | **188/188** | 162/188 |
| `i_` — suite abstains | 35 files | 30 accepted, 5 rejected | same |
| crashes (never acceptable) | — | **0** | 0 |

`i_structure_500_nested_arrays.json` is rejected: 500 levels is past the nesting limit —
a policy decision, not a conformance loss (README "Resource Limits"). The suite abstains
on `i_` either way.

## Nesting depth — bounded, on the way in

Recursion cannot fail gracefully. An unbounded recursive-descent parser exhausts the C++
stack and dies with SIGSEGV — not an exception, so nothing can catch it. Measured
2026-09-16 (gcc, x86-64) without a depth bound:

| input | 8 MB main-thread stack | 1 MB worker-thread stack |
|---|---|---|
| 20,000 nested arrays | parses | SIGSEGV at ~3,000 levels |
| 24,000 nested arrays | SIGSEGV | SIGSEGV |
| 100,000 `[`, closers not needed | SIGSEGV | SIGSEGV |

30,000 nested `[` closed by 30,000 `]` is **valid JSON**, and ~60 KB is enough to kill a
process — a remote DoS for anything parsing JSON from off-machine. Arrays, objects and
mixed nesting behave alike. RFC 8259 sets no depth limit, so the suite files the
pathological cases under `i_`: rejecting them with a proper error is conformant, and so
is parsing them iteratively. Dying is not.

**The bound**: one integer comparison per container, checked on the way *in*.
`JsonParseOptions::max_depth` (default `limits::MAX_NESTING_DEPTH` = 256) governs parsing,
and the same limit governs every traversal — serialization, comparison, path listing and
formatting — so a document JSOM accepts is a document JSOM can print, compare and walk.
Deeper input, including `[`*100000 with no closers, is rejected:

```bash
python3 -c "print(chr(91)*30000 + chr(93)*30000, end='')" > /tmp/deep.json
./jsom format /tmp/deep.json        # Error: Maximum nesting depth exceeded (limit 256)
```

```text
Maximum nesting depth exceeded (limit 256)
```

Parsing, serialization and comparison throw `jsom::ParseError` (a `std::runtime_error`
carrying `ParseErrorCode::NestingDepthExceeded`); existing code that catches
`std::runtime_error` around these calls needs no changes.

The limit comes from measured stack cost per level (~0.3–0.6 KB, worst case an
unoptimised debug build), so the deepest legal document fits a 1 MB thread stack with
room to spare; `tests/test_nesting_limits.cpp` runs the full set of traversals at exactly
the limit on a real 1 MB thread.

What matters is the caller's stack, not a bracket count: the same document parses at
20,000 levels on an 8 MB main thread and dies at ~3,000 levels on a 1 MB worker thread,
and the threshold moves with compiler and optimisation level. That is why the limit is
explicit and configurable instead of left to luck.

Fuzzing reaches it cheaply: fuzz inputs are capped at 4 KB, and the limit (256) is
reachable from a **256-byte** input, so ordinary fuzzing exercises the guard.
`fuzz/seeds/deep_nesting.json` (400 levels, 800 bytes) is checked in for exactly that.

## Numbers and escapes — what is rejected, and when

The suite's remaining disagreements fall into two groups, both decided explicitly.

**Escapes, control characters, whitespace — enforced ALWAYS, not a setting.** §7 requires
control characters to be escaped inside strings and defines the escape set exactly; §2
defines whitespace as space/tab/LF/CR. Four rules, enforced regardless of options:

1. a raw control character (U+0000..U+001F) inside a string → `Unescaped control character in string`
2. `\u` without exactly four hex digits → `Invalid hex digit in unicode escape`
3. any escape outside `" \ / b f n r t u` → `Invalid escape sequence: \U`
4. formfeed/vertical tab as whitespace → rejected (`std::isspace()` is locale-dependent, so the set is spelled out)

Rule 3 means an escape's backslash is never dropped: an unrecognised escape is a syntax
error, not a character that loses its prefix, so no accepted document is altered.
`LexicalConformanceTest.NothingIsDroppedFromAnAcceptedDocument` pins that.

**Numbers — enforced by default, and it costs nothing.** The §6 grammar is checked while
scanning, so there is no second pass: measured **0.92x–0.98x** against not checking it,
i.e. slightly *faster* (OPTIMIZATIONS.md, "Number grammar"). RFC 8259 §9 permits accepting
non-JSON forms, which is what `JsonParseOptions::allow_loose_numbers` /
`ParsePresets::Loose` / `--validation=loose` is for — an opt-in extension mode, not the
default. Validation does not force conversion: `LazyNumber` still keeps the original text,
so `1.500` round-trips byte-exact.

Cost of the always-on lexical rules: **+1.5% on string-heavy parsing**, everything else
within noise (OPTIMIZATIONS.md, "Lexical rules").

### The switch

```bash
./build/jsom_conformance                          # §6 enforced (default) -> n_ 188/188
./build/jsom_conformance --validation=loose       # extensions accepted   -> n_ 162/188
./jsom validate --validation=loose file.json      # the same switch on the CLI
```

`ParsePresets::Loose` (or `JsonParseOptions::allow_loose_numbers`) is the API spelling.
RFC 8259 §9's permission to accept non-JSON forms is what makes the leniency a documented
extension rather than an oversight — but it is opt-in, because an input that is not JSON
should not be reported as JSON unless the caller asked for tolerance.

**Reference point for the policy**: on the same corpus, nlohmann/json 3.11.3 scores
`y_` 95/95, `n_` **187/188** (one disagreement, a NUL after digits), `i_` 7 accepted /
28 rejected. With the number grammar enforced by default, JSOM is clean on all 188 `n_`
files where nlohmann misses one; the two differ only on the `i_` list, where the suite
deliberately has no opinion.

## Round-trip fidelity — what holds

`tests/fuzzer.cpp` asserts `parse(to_json(doc)) == doc` for everything either
configuration accepts — the default and `--validation=loose` (measured 2026-09-16:
`fuzz_quick` green, 325,624 runs, no artifacts). What the default mode does with the
shapes that stress the round trip:

| document containing | parse | round trip |
|---|---|---|
| a raw NUL, tab or newline inside a string | rejected (rule 1) | not applicable |
| escaped `\u0000` | accepted | stable |
| a valid `\uXXXX` | accepted | stable |
| an escape outside the set (`\U0041`) | rejected (rule 3) | not applicable |

So the oracle holds for every accepted input: the shapes that make a document change
identity are syntax errors, and the serializer and the parser agree on the form of
everything else.

One documented limitation, kept because it is a genuine asymmetry rather than a bug: in
fidelity mode (`convert_unicode_escapes = false`) the serializer writes a raw control
character as the six-character text `\uXXXX`, and fidelity parsing reads that text back
as *text* rather than as the character. It is unreachable through the parser — rule 1
rejects raw control characters — so it cannot affect the round trip of an accepted
document. Making it re-readable would mean either emitting raw bytes (invalid JSON) or
decoding `\uXXXX` (the `convert_unicode_escapes` mode, where the character round-trips
as expected).

## Next steps

1. `run_conformance` is in the gate set: the `conform` stage asserts the DEFAULT verdict
   (`n_` 188/188) and reports the loose-mode count. Both the number grammar and the lexical
   rules are enforced by default, so the default verdict is the meaningful one.
2. Everything else in the suite's judged classes is clean; the remaining `i_` files are
   where the suite has no opinion and JSOM takes its documented positions (see the
   `i_` list from `conformance_report`).
