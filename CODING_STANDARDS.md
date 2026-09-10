# JSOM — C++ Coding Standards (MANDATORY for all agents and humans)

This file is the binding standard for all C++ work in this repository. It
implements the *C++ Core Guidelines* (Type / Bounds / Lifetime profiles) in a
form that is machine-checkable and agent-followable. Exceptions are ALLOWED
for error handling — this is not MISRA/JSF.

Reference: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines

## Hard rules (no exceptions)

1. **C++17 minimum.** No C-style code in new work.
2. **No raw owning pointers.** Use `std::unique_ptr` / `std::shared_ptr` and
   RAII. A raw pointer (or reference) is a *non-owning view* only — never
   delete through it, never store it beyond the owner's lifetime.
3. **No `new` / `delete`**, no `malloc` / `free` in project code.
4. **Views over ranges:** `std::span` for contiguous buffers, `std::string_view`
   for read-only string parameters, instead of pointer+length.
5. **No `reinterpret_cast` or C-style casts.** `const_cast` only with a comment
   explaining why. Prefer `static_cast` / `dynamic_cast`.
6. **No undefined behavior:** no reliance on signed overflow, no out-of-bounds
   access (use `.at()` or checked access where indexing is dynamic), nothing
   read uninitialized — initialize every variable at declaration.
7. **`[[nodiscard]]`** on accessors and pure functions (e.g. `as_double()`,
   `as_int()`, `is_integer()`). If a call site must discard the value, cast to
   `(void)` with a comment — as in `test_lazy_number.cpp`.
8. **Exceptions are for error handling only**, never control flow. RAII
   guarantees cleanup on throw. Throw `TypeException`/documented exception
   types, never built-ins.
9. **No global mutable state.** Threaded code must be race-free: mutex/atomic,
   prefer immutable data. (JSOM is single-threaded today — keep it that way
   unless there is a measured reason.)
10. **Never keep iterators or references across container mutation.** Re-fetch
    after `set()` / `push_back()` / erase (the Lifetime profile's core rule).
11. **Zero warnings.** Project targets compile with
    `-Wall -Wextra -Wpedantic -Werror` (see CMakeLists.txt — deps like gtest
    are exempt; your code is not).
12. **TDD.** Write the failing test first, watch it fail, implement, watch it
    pass. Bug fixes too: failing test that isolates the bug → fix → green.
13. **Sanitizer gate:** all tests must pass under ASan+UBSan before a change is
    done:
    ```bash
    cmake -B build-asan -DJSOM_SANITIZE=ON
    cmake --build build-asan -j$(nproc)
    ./build-asan/jsom_tests
    ```

## Style (mechanical, enforced by clang-format)

- Follow the existing clang-format config; run `make format` before finishing.
- Names: snake_case functions/variables, PascalCase types, `jsom::` namespace.
- Keep the naming philosophy in CLAUDE.md: JSON = notation, JSOM = models.

## Definition of done (agent checklist)

- [ ] `cmake --build build` — zero warnings (`-Werror`)
- [ ] `./build/jsom_tests` — all tests pass
- [ ] `./build-asan/jsom_tests` — clean under ASan+UBSan
- [ ] `make tidy` — no NEW clang-tidy findings vs the baseline
- [ ] Fuzzing: input-handling changes run the fuzz targets briefly
      (`cmake --build build --target fuzz_quick`); a crash is a bug — fix it,
      add a regression test, keep the reproducer
- [ ] Performance: any change to a hot path (parser, serializer, DOM ops) is
      MEASURED, not assumed — interleaved A/B with `tools/perf_probe.cpp`
      (see OPTIMIZATIONS.md; sequential before/after runs are noise)
- [ ] No raw owning pointers / `new` / `reinterpret_cast` introduced
- [ ] Test written first (RED) for every behavior change or bug fix
