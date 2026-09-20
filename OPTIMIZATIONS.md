# JSOM — Performance Optimization Survey

Status: **#1, #1b, #2 IMPLEMENTED (`5778292`, `a343ce1`) — #3, #4, #6
IMPLEMENTED (`02482aa`). #5, #7, #8 still proposals.** Everything below that is not marked implemented is still open for
evaluation before any code changes land.

---

## Method

- Release build (`-O3 -march=native`), the standalone probe
  `tools/perf_probe.cpp` (in-repo since 2026-09-09), `perf` sampling profile
  on the MacBook.
- Measurement rule: **sequential before/after runs are noise (±5%)** — every
  claim below comes from a *controlled interleaved A/B*: two probe binaries
  (current vs one temporary reverted hunk) alternating A/B/A/B in one session,
  3 rounds, spread reported. Temporary measurement patches are reverted.
- **Probe input shape decides what you can see**: 9-char keys are SSO (the
  removed copy is already free) — long keys are needed to expose key-handling
  costs. The probe now includes a realistic 20-30-char-key workload.
- Baselines and profile were first captured 2026-09-09.

### Baselines (unmodified code, `-O3 -march=native`)

| Workload | Time | Effective rate |
|---|---|---|
| Parse 2000-key object (32 KB) | 635 µs | ~19 MB/s |
| Parse 2000-number doc (17 KB) | 319 µs | ~55 MB/s |
| Parse 1000-string doc (44 KB) | 182 µs | ~240 MB/s |
| Parse depth-300 nesting (602 B) | **1910 µs** | pathological |
| Serialize 2000-key object (32 KB) | 83 µs | ~400 MB/s |
| Serialize 2000-number doc (17 KB) | 22 µs | ~800 MB/s |

### Profile summary (top self-costs)

- `malloc` + `free`: ~20%
- `std::__do_uninit_copy` over `vector<JsonDocument>` (deep copies): ~11%
- `std::variant` reset/destruction machinery: ~7%
- `std::map` insert/rebalance: ~1% self (plus its allocation share of the
  malloc/free total)

---

## Tier 1 — tiny change, measured 76× win

### 1. Arrays deep-copy every parsed element (the deep-nesting killer) — ✅ IMPLEMENTED

**Finding.** `JsonDocument::set(std::size_t, const JsonDocument&)` is the only
array overload; there is no rvalue overload. `parse_array` therefore calls
`result.set(index++, parse_value())` with a prvalue that binds to `const&`,
**deep-copying each element**, after which the temporary is destroyed. Nested
values pay a full subtree copy per element — O(n²) in nesting depth.

**Fix.** Add `set(std::size_t, JsonDocument&&)` — 5 lines, an exact mirror of
the existing `const&` overload with `std::move(value)`. The class already has
four `set` overloads (object + array × `const&` + `&&`); this completes the
existing pattern rather than introducing a new concept. *(Done in commit
`5778292`; regression test `DeepNestingParseIsLinear` added first — RED at
3181 ms, GREEN at 1 ms.)*

**Complexity / risk: negligible.**
- No API change: lvalue callers still bind `const&` with identical semantics.
- No observable behavior change: the rvalue argument is a temporary that was
  destroyed either way; moving is invisible to all callers.
- No overload ambiguity: `&&` wins for rvalues, `const&` for lvalues.
- No new dependencies, no noexcept concerns (JsonDocument's move operations are
  already `noexcept`).

**Measured impact** (temporary patch, reverted after measurement):

| Workload | Before | After | Speedup |
|---|---|---|---|
| Parse depth-300 nesting | 1910 µs | 25 µs | **76×** |
| Parse 2000-number doc | 319 µs | 248 µs | 1.3× |
| Parse 1000-string doc | 182 µs | 168 µs | 1.08× |
| Parse 2000-key object | ~635 µs | ~640 µs (noise) | — (map-bound, see #7) |

---

### 1b. parse_array default-constructs a null document per element — ✅ IMPLEMENTED (`a343ce1`)

**Finding.** With #1 in place, array parse still does
`set(index++, value)` → `resize(index+1)` **default-constructs a null
JsonDocument** at each new slot, then move-*assigns* the parsed value over it.
Per element: one null construction + one move-assign instead of a single
move-construction. The number-heavy probe (2000-element array) spends a large
share of its ~250µs here — estimated 25-40% recoverable.

**Fix candidate.** Build arrays with `push_back`/`emplace_back` style growth
(move-construct in place; vector growth moves are `noexcept`). Requires a
private/guarded array-builder path (no public API change).

**Complexity:** very small (no new API — `push_back` already existed). **Risk:**
low. **Status:** ✅ implemented. **Measured A/B** (interleaved, 3 rounds, tight
spread — ~0.3% run-to-run): numbers 108.7→98.4 ms (**−9.5%**), strings
64.2→56.8 ms (**−11.5%**), deep-300 10.1→8.5 ms (**−16.1%**). Estimate
(25–40%) was optimistic again — reality ~10–16% — but this one is solidly
above noise and reproducible.

## Tier 2 — small changes, solid wins

### 2. Default builds compile at -O0 (biggest real-world win) — ✅ IMPLEMENTED

**Finding.** `CMAKE_BUILD_TYPE` is not defaulted in CMakeLists, so the
documented `cmake -B build` produces an **unoptimized** library. Everything
built that way — the CLI, the tests, downstream users — runs at -O0.

**Fix.** Default `CMAKE_BUILD_TYPE` to `Release` when unset (or add a `-O2`
fallback for the library when no build type is given). ~3 lines. *(Done in
commit `5778292`. Consequence handled: the Release default surfaced a gcc<15
`-Wmaybe-uninitialized` false-positive family on `std::variant` internals
under optimized+NDEBUG test builds (gcc#101905); suppressed for `jsom_tests`
only, version-gated to GNU < 15, with a citation comment — library and CLI
keep full `-Werror` on all compilers.)*

**Complexity:** none. **Risk:** none (an explicitly chosen build type still
wins). **Impact:** 5–20× on all default builds, zero code change.

#### Scope of the default — JSOM's own builds only (2026-09-20, card t_772eabd9)

**What went wrong.** `CMAKE_BUILD_TYPE` is a CACHE variable, and a cache write with
`FORCE` is global to the whole CMake run — so the three lines above did not stay in
JSOM's scope. JSOM enters a consumer as a SUBDIRECTORY (`FetchContent_MakeAvailable`,
`add_subdirectory`), so **JSOM was configuring its consumers**: Computo's Pages workflow,
which passes no build type (exactly like `cmake -S . -B build`), inherited `Release` and
compiled at `-O3` instead of CMake's `-O0`, and JSOM's own `-march=native` Release flags
made it link objects built for the builder's CPU. That is how a gcc 13/14
`-Wmaybe-uninitialized` false positive in libstdc++'s `<variant>` machinery took Computo's
only production target offline for ~7 months (card t_93e9a66b, INCIDENTS there).

**The decision (Harri, 2026-09-20):** keep the default — it is this section's whole point —
but make it **top-level only**. A subproject must not be able to reconfigure its consumer.

**The fix.** Both cache writes here are now gated on `JSOM_IS_TOP_LEVEL`, which is detected
above them: `cmake -B build` in this repo still gets `Release` (no documented flow changes),
and a consumer that asks for nothing now gets CMake's defaults. The **non-`FORCE` form is
not** the fix — measured, not argued: a plain `set(CMAKE_BUILD_TYPE Release CACHE STRING
"Build type")` is a no-op everywhere, because CMake has already created an empty
`CMAKE_BUILD_TYPE:STRING=` cache entry by the time the script runs, so the non-forced
`set()` finds an entry and leaves it. It would silently delete this default and fix nothing.

**Same edit, same class of leak (both `FORCE`, both fixed the same way):**
`CMAKE_INSTALL_PREFIX` was rewriting the CONSUMER's install prefix to `$HOME/.local` (a
consumer that installs under the default would install into the builder's home), and
`CMAKE_EXPORT_COMPILE_COMMANDS` — JSOM's own clang-tidy/IDE aid, so it is JSOM's own build
that asks for it now. As a subdirectory it wrote a consumer's `compile_commands.json` with
JSOM's translation units and NONE of the consumer's own, which a tidy gate reading that file
would silently lint in place of the consumer's sources. A consumer that wants the database
asks for it itself (jsonTools' and Computo's gates both pass
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`), and JSOM's TUs are then still in it, because the
consumer's directory scope reaches this subdirectory.

**Kept honest by a stage, not by intent:** `tools/consumer_probe.sh`, run as the gate's
`consumer` stage on every run — 9 configure-only, offline checks over stub consumers
(FetchContent and `add_subdirectory` shapes) asserting the cache stays theirs and that
Release still lands for this repo's own build. 5 of those checks fail on the pre-fix tree
(measured against `git archive` of the commit before this one), so the probe has teeth.

**Deliberate asymmetry, recorded rather than normalised:** JSOM's gate builds `Release`
(`-O3 -DNDEBUG -march=native`), while the AI-DEV-STARTER gate template and Computo default
their gates to `Debug`. This is the section's default doing its job, not an oversight.
The gcc<15 `-Wmaybe-uninitialized` suppression stays `PRIVATE` on `jsom_tests` and does not
widen: it cannot reach a consumer TU (widening it means `INTERFACE`, i.e. the very leak this
fixes), and JSOM's own non-test translation units already compile at `-O3` with the full
warning set and zero warnings (measured on this box, gcc 14.2, 2026-09-20).

**Companion, in jsonTools:** its gate passed no build type and its `Release` build directory
existed only by inheriting this leak — so the guard alone would have dropped it to `-O0`
silently. Its `tools/ci.sh` now sets `CI_BUILD_TYPE=Release` explicitly and passes it to
every configure call, i.e. the same configuration as before, but chosen.

### 3. Object keys round-trip through a JsonDocument — ✅ IMPLEMENTED (`02482aa`)

**Finding.** `parse_object` parses each key via `parse_string()` (returns a
`JsonDocument`), then copies the string out with `key_doc.as<std::string>()`,
then destroys the document. Per key: one extra string allocation, copy, and
free.

**Fix.** Internal `parse_string_raw()` (or equivalent) that returns
`std::string` directly, reusing the existing scan logic. Key parsing in
`parse_object` calls that instead.

**Complexity:** small — refactor of an existing private helper; no API change.
**Risk:** low. **Impact:** ~15–25% on key-heavy objects (estimate; measure).

### 4. Number scan calls `std::isdigit` per character — ✅ IMPLEMENTED (`02482aa`)

**Finding.** The number-token scan in `parse_number` calls `std::isdigit(c)`
(plus a compare chain) per character — a locale-table function call in the hot
loop. The scan is also syntax-lax: tokens like `1e+` or `1.2.3` are accepted by
the scanner (correctness surfaces only later, if at all).

**Fix.** Replace the `isdigit` chain with direct comparisons or a 256-entry
lookup table, and tighten the token shape check while there.

**Complexity:** none (table) / small (shape validation). **Risk:** low; watch
that valid exponents and negatives still parse (test suite covers this).
**Impact:** small on number-heavy parses; plus a correctness tightening.

### 5. `LazyNumber::as_double` uses `std::stod`

**Finding.** `std::stod` is locale-dependent, uses exception machinery per
call, and re-parses the whole string. It also *accepts* `inf`/`nan`/hex forms
that are not valid JSON.

**Fix.** `std::from_chars` (C++17 `<charconv>`, needs libstdc++ 11+ / MSVC /
libc++ 14+): locale-independent, ~2–4× faster, exception-free, and more
correct for JSON (rejects `inf`/`nan`/hex that `stod` accepts).

**Complexity:** small (localized to `as_double`). **Risk:** low — rounding
follows the same shortest-round-trip rules; verify against the existing number
test suite. **Impact:** big for workloads that *read* numbers (pointer math,
queries, comparisons); none for parse-only workloads (numbers are lazy).

### 6. `set(const std::string&, JsonDocument&&)` default-constructs a null first — ✅ IMPLEMENTED (`02482aa`)

**Finding.** The object rvalue path uses `map[key] = std::move(value)`:
`operator[]` default-constructs a null `JsonDocument` at the key, then
move-assigns the value over it. The sibling overload
`set(std::string&&, JsonDocument&&)` already does it right with
`insert_or_assign`.

**Fix.** Use `insert_or_assign(key, std::move(value))` in the `const
std::string&` overload too. 1 line.

**Complexity:** none. **Risk:** none (same semantics). **Impact:** modest on
large-object construction (one null construct + node churn per key).

*(Measured. On the synthetic short-key/SSO workloads all three land **within
noise** (objects ~652µs vs ~642µs baseline). On a REALISTIC workload —
2000 non-SSO keys ("application_configuration_key_N", 80KB object) — a
controlled A/B shows #3's move-out at **~809µs vs ~833µs reverted (~3%
faster)**, consistent across runs. #4/#6 remain unmeasurable in isolation
(zero API cost, strictly fewer operations — kept on that basis). Original
estimates for these items (15–25%) were too optimistic by several-fold;
measured reality is 0–3%. See #1b for the next measurable array win.)*

---

## Tier 3 — structural, needs a design conversation

### 7. Object storage is `std::map<std::string, JsonDocument>`

**Finding.** A heap node allocation per key + O(log n) string comparisons
(visible in the profile as `_Rb_tree_insert_and_rebalance` and a large share of
the ~20% malloc/free churn). This is the remaining wall for object-heavy
parses (~19 MB/s).

**Options.**
- `std::unordered_map`: typically 2–4× on insert/lookup; loses deterministic
  sorted iteration (serialization output order changes).
- Flat `vector<pair<string, JsonDocument>>` (optionally sorted + binary
  search): cache-friendly, near-zero per-key allocations, often best for
  small/medium objects.
- Both are **API-visible** (`as_object()` currently returns a `map&`), so this
  needs a design decision (container swap vs. internal storage + map facade)
  before implementation.

**Complexity:** medium (API surface). **Risk:** medium (format/order
sensitivity). Worth a separate experiment against real workloads before
choosing.

### 8. Whole-DOM allocation churn

**Finding.** ~20% of profile time in `malloc`+`free`: per-key nodes, per-value
storage, vector growth, and the recursive destructor cascade.

**Fix candidate.** A parse-time monotonic arena would remove most of it, but it
interacts with `map`/`variant` ownership and document lifetime — medium
complexity. **Defer** until items 1–7 land and the profile is re-measured.

---

## Micro notes (not recommended yet)

- `parse_string` scans char-at-a-time for quote/backslash; `memchr` fast paths
  (SIMD) could speed long strings. Moderate complexity for the win; revisit if
  string-heavy workloads matter.
- Serializer `reserve()` is small for large documents; serialization is
  already ~400–800 MB/s — skip for now.
- Parser recursion depth: deep/adversarial input risks stack overflow. A
  robustness item (iterative parse), not a speed item.

---

## Depth guard (2026-09-16) — the cost of not dying

Bounding recursion (CONFORMANCE.md Finding 1) adds one comparison per container.
Measured on a 200-deep array: best of 7 inner rounds, 3,000 iterations, three builds
compiled from the same source and interleaved in one session (`-O3 -march=native`,
Release lib):

| build | ns per nesting level | vs baseline |
|---|---|---|
| baseline (no guard) | 69.2 | — |
| **guard via a member counter** | **70.4–70.8** | **+1.7%** |
| guard via a threaded `level` argument | 81.7–82.3 | +18.3% |

The first implementation threaded the level down the mutual recursion as a parameter —
the shape the traversal guards use, where it costs nothing — and in the parser that
extra argument costs ~12 ns per level, presumably by changing inlining and register
allocation across `parse_value` ↔ `parse_array`/`parse_object`. A member counter
(`++depth_` / `--depth_`, reset in `parse()`) measured ~1.7%, so that is what landed.

Shape matters more than the average: a pathological deep document spends ~70 ns per
level in total, so 1.5 ns is 2% there, while for realistic documents (depth ≤ 10) the
guard is unmeasurable against the work of parsing real content — the
`tools/perf_probe.cpp` A/B on number/string/object shapes stayed within noise (±3%).

The invariant that makes the counter safe: every normal return from `parse_object()` /
`parse_array()` decrements it, including the early return for an empty container, and
`parse()` resets it. A missed early return leaks one level per empty container — pinned
by `NestingLimitTest.EmptyContainersDoNotConsumeTheNestingBudget`.

---

## Number validation (2026-09-16) — what strictness costs

`JsonParseOptions::validate_numbers` (default **off**, `ParsePresets::Validate` turns it on)
checks the RFC 8259 §6 number grammar over the text the scan already collected. Measured
on a Release build, 5 rounds per case reporting the best, two builds compiled from the
same probe and run in the same session:

| input | default (lazy) | validation on | delta |
|---|---|---|---|
| 2,000 short numbers (`123.45e3`) | 0.240 ms | 0.255 ms | **+6%** |
| 1,000 × 17-digit numbers with exponents | 0.115 ms | 0.129 ms | **+12%** |
| 1,000 realistic mixed records | 1.37 ms | 1.38 ms | **+0.5%** |

Two things worth reading off that: the cost is proportional to *number text length*
(~1 ns per digit — it is a second pass over the collected bytes), and it disappears in
realistic payloads because numbers are a small fraction of the work. Turning validation
on for a document that is 100% digits costs about a tenth of what the parse itself costs
per number.

The cost of the *switch itself* on the default path is a predictable branch per number:
0.2374 → 0.2403 ms on the short-number case (+1.2%), within noise on the other two.
Number-heavy benchmarks are the worst case for reading that off — for a mixed payload
the branch is invisible.

---

## Lexical rules (2026-09-16) — the conformance rules that are always on

Escape validation, control-character rejection and the whitespace set (CONFORMANCE.md
Finding 2) are unconditional, so unlike the number grammar they cost every parse. The
only path that pays is the string scan: one extra comparison per byte
(`if (c < 0x20) throw`), because the escape checks only fire on a backslash.

Interleaved A/B, 4 rounds per binary, same probe source, median reported:

| benchmark | before | after | delta |
|---|---|---|---|
| parse strings 1000 | 56.78 ms | 57.64 ms | **+1.5%** |
| parse objects 2000 | 266.1 ms | 262.0 ms | −1.5% (noise) |
| parse longkey obj 2000 | 245.9 ms | 236.7 ms | −3.7% (noise) |
| parse numbers 2000 | 91.3 ms | 88.1 ms | −3.5% (noise) |
| parse deep 200 | 5.75 ms | 5.60 ms | −2.7% (noise) |
| serialize objects 2000 | 33.10 ms | 33.38 ms | +0.8% (noise) |

So the honest number is **+1.5% on string-heavy input, nothing measurable elsewhere**.
Two things kept it that cheap, and both are worth keeping if this ever grows: the check
is folded into the loop that already reads each byte (no second pass), and it is a single
unsigned comparison rather than a table lookup or a `std::isspace` call. If a future rule
needs more than one predicate per byte, the alternative is a 256-entry class table with
one load and test per byte — measure both before choosing.

---

## Path cache removed (2026-09-19) — measured, then deleted

Every `JsonDocument` used to own a lazily-created three-level path cache (exact paths,
prefixes, recent prefixes with 10-minute aging) that `at()`, `find()`, `exists()` and
`at_multiple()` fed on every lookup. It had never been measured. Measured, it lost:

`tools/cache_probe.cpp` (in git history at `3ee8251^`), `-O3 -march=native`, 100 KB
document of 1000 records, median of 5, cached `at()` vs `NavigationEngine::navigate_simple`
on the same paths:

| pattern | cached | uncached | ratio |
|---|---|---|---|
| repeat one shallow path ×10000 | 2.640 ms | 2.649 ms | 1.00× |
| shared-prefix sweep (1000 leaves) | 0.303 ms | 0.282 ms | 1.07× |
| every path once (7003 paths) | 33.810 ms | 1.881 ms | **17.97×** |
| repeat one 200-deep path ×10000 | 42.909 ms | 69.835 ms | 0.61× |
| parse + one lookup | 1.032 ms | 1.250 ms | 0.83× |
| write + read ×2000 | 9.547 ms | 8.334 ms | 1.15× |

Reading every path once — the shape a consumer of a document actually has — was **18×
slower** with the cache: each lookup misses exactly, then inserts into the exact map, the
prefix map and the LRU order, and caches every intermediate node (1000-leaf sweep → 1000
exact + 1001 prefix entries, ~72 KB, for a document that was ~60 KB of text). The one win
was repeating the *same deep* path (1.6×), which a caller gets by holding the pointer from
the first lookup.

What deleting it removed, beyond the perf loss:

- **a data race, demonstrated not theorised**: the cache lived behind `mutable` members
  and was reached from const methods through `const_cast<JsonDocument*>(this)`, so
  `doc.at(p)` on a shared const document wrote to it. TSan on four threads reading one
  document reported **71 data races and then a SEGV inside `memmove`**; after removal the
  same test is clean, and `thread_safety_probe` now gates it in ~6 s.
- a **raw owning pointer** (`new PathCache()` / `delete path_cache_`) — against the repo's
  own rule 5 — plus a process-global `s_mutation_epoch_` that every mutation bumped, and
  cached raw pointers into document storage that could dangle when a child vector grew.
- wall-clock eviction (`steady_clock`, `MAX_PREFIX_AGE_MINUTES`) inside a core data
  structure, and the whole `cache_constants` block.
- the `NOLINT(bugprone-empty-catch)` pair and much of the `readability-function-size`
  suppression weight.

Replaced by: `NavigationEngine::find(const JsonDocument*, path)` (one implementation, one
`const_cast` in the mutable overload, so no caller needs its own), `find_multiple()` for
batches, and a documented lifetime contract — a returned reference dies at the next
mutation. `tests/test_navigation_freshness.cpp` (9 tests, from the old cache-invalidation
suite) keeps the property that matters: every lookup sees the current document.

---

## Status summary / what remains

**Done:** #1 (76× deep nesting), #1b (−9.5/−11.5/−16.1% arrays), #2 (Release
default), #3/#4/#6 (0–3%, behavior-neutral cleanups), depth guard for hostile
input (+1.7% on the deep path — see the section above).
**Remaining, in priority order:**
1. **#7 — object storage (`std::map`).** The last measured wall: object parse
   is now the slowest shape (~19 MB/s) and is map-bound. Needs a design
   decision first: serialization key order (today sorted via `std::map`;
   flat storage = insertion order). Recommend: prototype flat storage +
   binary search behind the existing API, A/B on object-heavy workloads,
   decide order semantics with the format tests in place.
2. **#5 — `from_chars` in `as_double`** if number-ACCESS workloads
   (Computo-style querying) become real; zero payoff for parse/serialize.
3. **#8 — arena allocation** only if the profile still shows malloc/free
   dominance after #7.

---

*Measurement probe: `tools/perf_probe.cpp` (in-repo; header documents the
build command and the interleaved-A/B rules). Profile: `sudo perf record -F
4000 -g` on the probe looped 6x (paranoid=3 blocks unprivileged profiling).
All numbers reproducible with a Release build + `-O3 -march=native`. The
google-benchmark suite (`./build-rel/jsom_benchmarks`, `JSOM_BUILD_BENCHMARKS=ON`)
is the complementary end-to-end harness — note it needed the stale
`benchmark_dom_access_compat.cpp` construction block repaired (2026-09-09) to
build at all.*
