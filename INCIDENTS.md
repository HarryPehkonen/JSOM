# INCIDENTS — every real failure, and the check that now catches it

Newest first. One entry per incident that changed how this repo works.

The rule: **when something breaks, the fix is not done until a check exists that would
have caught it, and the incident is written here next to that check.** A check with no
written rationale looks arbitrary to the next hurried contributor (or agent), and
arbitrary checks get deleted. The rationale is the load-bearing part.

    ## YYYY-MM-DD — <one-line failure>
    What broke:        <the user-visible symptom>
    Check added:       <file> + <gate stage that now catches it>
    Why it must stay:  <why deleting this check re-enables the bug>

---


## 2026-10-06 — the gate is no longer a 768-line bash script (wiring record, not a breakage)

What changed:      `tools/ci.sh` is deleted. The gate is now `gate.toml` — the policy: 14 stages
                   in two tiers — run by `kit-ci`, one binary installed once per machine
                   (`cmake --install build --prefix ~/.local`), plus `scripts/gate-env.sh` (the
                   .ci.env knobs and the helpers every stage reads), `scripts/gate.sh` (the one
                   file all three callers run) and one `scripts/<stage>.sh` per stage. The two
                   hooks are the kit's files with one line changed each: they NAME the tier
                   instead of repeating a stage list. This entry is a wiring record, not an
                   incident — it is here because the hooks and the retired gate both say "if you
                   edit this file, say why in INCIDENTS.md".
Check moved:       Every stage the old gate ran is still run, under the same name and in the
                   same order. full tier: tree format build tests consumer asan fuzz tsan std cli conform docs tidy pristine. fast tier: format build tests.
                   The teeth were re-measured on the converted gate rather than assumed:
                   an unformatted new source file dropped in the tree makes the fast tier
                   report GATE FAILED naming the `format` stage — captured in
                   `gate-evidence/t_075c0a6f/JSOM-teeth.txt`.
Probes:            the `kitprobes` stage and `tools/kit-probes/` are DELETED. A probe checks a
                   FILE, and the file it checked (tools/ci.sh) is gone; a stage whose verdict is
                   an accident of how a probe searches is the failure mode the gate exists to
                   prevent. Measured 2026-10-06, each probe from HEAD run against the new entry
                   point `scripts/gate.sh`:
                   * format-checks-staged.sh      GREEN against scripts/gate.sh
                   * gate-stage-guards.sh         RED against scripts/gate.sh
                   * git-index-file.sh            GREEN against scripts/gate.sh
                   * hook-tiers-agree.sh          RED against scripts/gate.sh
                   * tidy-baseline.sh             RED against scripts/gate.sh
                   The guarantees themselves did not go:
                   * format-checks-staged      -> scripts/format.sh — the staged-copy check is the same code, in the stage that owns formatting
                   * gate-stage-guards         -> kit-ci's own runner: a stage whose command does not exit 0 fails the run, so a green run over stages that never ran is not expressible
                   * git-index-file            -> scripts/gate.sh and scripts/gate-env.sh — `unset GIT_INDEX_FILE`, in both places it has to reach
                   * hook-tiers-agree          -> gate.toml — the tier lists are [tier.fast] and [tier.full], the hooks NAME a tier, and `kit-ci --list` prints what each one holds
                   * tidy-baseline             -> scripts/gate-env.sh, tidy_key() — applied to both sides in scripts/tidy.sh and used by scripts/write-tidy-baseline.sh to capture the file
                   and gate-stage-guards is subsumed by the engine's runner (a stage whose
                   command does not exit 0 fails the run).

---
