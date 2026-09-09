# JSOM — Performance Optimization Survey

Status: **proposal — nothing in this document is implemented yet.**
Scope: optimizations that make JSOM faster without significant increases in
code complexity. All items are listed with evidence, expected impact, and a
complexity/risk assessment so they can be evaluated and prioritized before any
code changes land.

---

## Method

- Release build (`-O3 -march=native`), scratch benchmark probe outside the
  repo, `perf` sampling profile on the MacBook.
- One temporary 5-line measurement patch was applied, measured, and **reverted**
  (tree clean at the time of writing). Every measured number below is from a
  real build of the *unmodified* code unless stated otherwise.
- Baselines and profile were captured on 2026-09-09.

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

### 1. Arrays deep-copy every parsed element (the deep-nesting killer)

**Finding.** `JsonDocument::set(std::size_t, const JsonDocument&)` is the only
array overload; there is no rvalue overload. `parse_array` therefore calls
`result.set(index++, parse_value())` with a prvalue that binds to `const&`,
**deep-copying each element**, after which the temporary is destroyed. Nested
values pay a full subtree copy per element — O(n²) in nesting depth.

**Fix.** Add `set(std::size_t, JsonDocument&&)` — 5 lines, an exact mirror of
the existing `const&` overload with `std::move(value)`. The class already has
four `set` overloads (object + array × `const&` + `&&`); this completes the
existing pattern rather than introducing a new concept.

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

## Tier 2 — small changes, solid wins

### 2. Default builds compile at -O0 (biggest real-world win)

**Finding.** `CMAKE_BUILD_TYPE` is not defaulted in CMakeLists, so the
documented `cmake -B build` produces an **unoptimized** library. Everything
built that way — the CLI, the tests, downstream users — runs at -O0.

**Fix.** Default `CMAKE_BUILD_TYPE` to `Release` when unset (or add a `-O2`
fallback for the library when no build type is given). ~3 lines.

**Complexity:** none. **Risk:** none (an explicitly chosen build type still
wins). **Impact:** 5–20× on all default builds, zero code change.

### 3. Object keys round-trip through a JsonDocument

**Finding.** `parse_object` parses each key via `parse_string()` (returns a
`JsonDocument`), then copies the string out with `key_doc.as<std::string>()`,
then destroys the document. Per key: one extra string allocation, copy, and
free.

**Fix.** Internal `parse_string_raw()` (or equivalent) that returns
`std::string` directly, reusing the existing scan logic. Key parsing in
`parse_object` calls that instead.

**Complexity:** small — refactor of an existing private helper; no API change.
**Risk:** low. **Impact:** ~15–25% on key-heavy objects (estimate; measure).

### 4. Number scan calls `std::isdigit` per character

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

### 6. `set(const std::string&, JsonDocument&&)` default-constructs a null first

**Finding.** The object rvalue path uses `map[key] = std::move(value)`:
`operator[]` default-constructs a null `JsonDocument` at the key, then
move-assigns the value over it. The sibling overload
`set(std::string&&, JsonDocument&&)` already does it right with
`insert_or_assign`.

**Fix.** Use `insert_or_assign(key, std::move(value))` in the `const
std::string&` overload too. 1 line.

**Complexity:** none. **Risk:** none (same semantics). **Impact:** modest on
large-object construction (one null construct + node churn per key).

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

## Suggested order of attack

1. **#1 + #2 first** — measured 76× on nesting; -O0→Release is free.
2. **#3–#6** as one small patch (all localized, all low-risk).
3. Re-measure — expect object-heavy parse 2–4× faster and array/nested-heavy
   docs 10–100× faster depending on shape.
4. **#7** as its own experiment with real workloads (the only item needing a
   design conversation). #8 only if the re-measured profile still demands it.

---

*Measurement probe: scratch harness in /tmp (not part of the repo). Profile:
`perf record` on the probe. All numbers reproducible with a Release build +
`-O3 -march=native`.*
