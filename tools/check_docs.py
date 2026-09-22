#!/usr/bin/env python3
"""Keep the documentation honest against the code.

Two checks, both of which caught real drift on 2026-09-21 (a README still advertising the
path cache deleted in 3.0.0, FORMATTING.md documenting a `pretty` option that had not
existed for months, and preset sizes that disagreed with the constants):

1. RETIRED IDENTIFIERS must not appear in documentation or headers. A line is allowed to
   mention one only when it is explicitly talking about the removal (it names
   OPTIMIZATIONS.md, or says removed/deleted/no longer/used to).
2. GENERATED BLOCKS must match the code. The option defaults and the preset tables are
   written from the real `JsonFormatOptions` initialisers and `FormatPresets` values, so a
   constant change fails this check until the docs are regenerated with `--write`.

Usage:
    tools/check_docs.py            # check (exit 1 on any mismatch)
    tools/check_docs.py --write    # regenerate the generated blocks in place
    tools/check_docs.py --verbose  # also list allowed historical mentions
"""
from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "FORMATTING.md"
CONSTANTS = ROOT / "include/jsom/constants.hpp"
OPTIONS = ROOT / "include/jsom/json_format_options.hpp"

# ---------------------------------------------------------------- retired things
RETIRED = [
    "validate_numbers",
    "ParsePresets::Validate",
    "--validation=lazy",
    "ValidationLevel",
    "PathCache",
    "path_cache",
    "precompute_paths",
    "warm_path_cache",
    "clear_path_cache",
    "get_path_cache_stats",
    "navigate_with_cache",
    "NavigationResult",
    "StreamingParser",
    "DocumentBuilder",
    "parse_document_streaming",
]
HISTORY_MARKERS = re.compile(
    r"OPTIMIZATIONS\.md|removed|removal|deleted|no longer|used to|would be|never caches"
    r"|no cache|don't re-add|do not re-add",
    re.IGNORECASE,
)
SCANNED_SUFFIXES = (".md", ".hpp", ".cpp", ".sh")
SKIP_PREFIXES = ("third_party/", ".ci/", "build", "corpus/", "fuzz/regressions/")


def tracked_files() -> list[str]:
    out = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True
    ).stdout.split()
    return [
        f
        for f in out
        if f.endswith(SCANNED_SUFFIXES) and not f.startswith(SKIP_PREFIXES)
    ]


def check_targets() -> list[str]:
    """Every build target the docs tell a reader to run must still exist.

    Found by hand on 2026-09-21: CLAUDE.md documents `fuzz_quick` / `fuzz` / `fuzz_long`,
    and a rename would leave the reader with a command that cannot work — nothing checked
    it. Candidates come from `--target X` and `make X` mentions.
    """
    builtin = {
        "all", "clean", "help", "install", "test", "package", "edit_cache",
        "rebuild_cache", "list_install_components", "install/strip", "uninstall",
    }
    # Only defined when the build enables JSOM_BUILD_FUZZING; documenting them is right, and
    # the fuzz stage builds exactly those.
    option_gated = {"fuzz_jsom", "jsom_fuzz_tests", "run_fuzz_tests"}
    defined = set(
        re.findall(r"add_(?:library|executable|custom_target)\(\s*([A-Za-z0-9_]+)",
                   (ROOT / "CMakeLists.txt").read_text())
    )
    patterns = [r"--target\s+([A-Za-z0-9_]+)", r"`make\s+([A-Za-z0-9_]+)`", r"^make\s+([A-Za-z0-9_]+)"]
    problems: list[str] = []
    for rel in ("README.md", "CLAUDE.md", "CODING_STANDARDS.md", "FORMATTING.md",
                "CONFORMANCE.md", "OPTIMIZATIONS.md"):
        path = ROOT / rel
        if not path.exists():
            continue
        text = path.read_text()
        for pattern in patterns:
            for hit in re.findall(pattern, text, re.MULTILINE):
                if hit in builtin or hit in defined or hit in option_gated:
                    continue
                problems.append(f"{rel}: names target '{hit}', which the build does not define")
    return problems


def check_retired(verbose: bool) -> list[str]:
    problems, allowed = [], []
    for rel in tracked_files():
        try:
            text = (ROOT / rel).read_text()
        except (UnicodeDecodeError, FileNotFoundError):
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            for token in RETIRED:
                if token in line:
                    if HISTORY_MARKERS.search(line):
                        allowed.append(f"{rel}:{lineno} mentions {token} as history")
                    else:
                        problems.append(
                            f"{rel}:{lineno} mentions retired {token!r}: {line.strip()[:90]}"
                        )
    if verbose:
        for line in allowed:
            print(f"  allowed: {line}")
    return problems


# ---------------------------------------------------------------- generated blocks
def read_constants() -> dict[str, str]:
    """Every integer constant in constants.hpp, including ones defined by reference
    (format_defaults::DEFAULT_MAX_DEPTH is limits::MAX_NESTING_DEPTH)."""
    src = CONSTANTS.read_text()
    return dict(re.findall(r"constexpr (?:int|std::size_t|size_t) (\w+) = ([^;]+);", src))


def resolve_constant(name: str, constants: dict[str, str], depth: int = 0):
    """Follow `a = b::C` chains to a number, so the docs show the value, not the alias."""
    if depth > 4:
        return None
    value = constants.get(name)
    if value is None:
        return None
    value = " ".join(value.split())
    m = re.fullmatch(r"(\w+)::(\w+)", value)
    if m:
        return resolve_constant(m.group(2), constants, depth + 1)
    return int(value) if value.isdigit() else None


def resolve(token: str, constants: dict[str, str], limits=None):
    token = token.strip().rstrip(",")
    if token == "std::nullopt":
        return None
    if token in ("true", "false"):
        return token
    m = re.fullmatch(r"(?:format_defaults|limits)::(\w+)", token)
    if m:
        value = resolve_constant(m.group(1), constants)
        return value if value is not None else f"<unresolved {token}>"
    return int(token) if token.isdigit() else token


def option_defaults():
    src = OPTIONS.read_text()
    constants = read_constants()
    limits = {"MAX_NESTING_DEPTH": constants.get("DEFAULT_MAX_DEPTH", 256)}
    body = src.split("struct JsonFormatOptions {", 1)[1].split("\n};", 1)[0]
    # Keep the initialiser with its field name even when it wraps onto the next line.
    fields = re.findall(
        r"^\s+(std::optional<int>|bool|int)\s+(\w+)\s*=\s*([^;/]+?);",
        body,
        re.MULTILINE | re.DOTALL,
    )
    rows = []
    for type_name, name, value in fields:
        rendered = resolve(" ".join(value.split()), constants, limits)
        if rendered is None:
            shown = "none (compact)"
        elif isinstance(rendered, str) and rendered in ("true", "false"):
            shown = "on" if rendered == "true" else "off"
        else:
            shown = str(rendered)
        rows.append((name, type_name, shown))
    return rows


def preset_rows():
    src = OPTIONS.read_text()
    constants = read_constants()
    limits = {}
    presets = []
    for name, block in re.findall(
        r"inline const JsonFormatOptions FormatPresets::(\w+) = \{(.*?)\n\};", src, re.S
    ):
        values = {}
        for line in block.splitlines():
            m = re.match(r"\s*([^/]+?),\s*//\s*(\w+)", line)
            if m and m.group(2) in {
                "indent_size", "sort_keys", "max_inline_array_size", "max_inline_object_size",
                "max_inline_string_length", "max_line_width", "align_values", "colon_spacing",
                "bracket_spacing", "quote_keys", "trailing_comma", "escape_unicode",
                "intelligent_wrapping",
            }:
                values[m.group(2)] = resolve(m.group(1), constants, limits)
        presets.append((name, values))
    return presets


def bool_cell(value) -> str:
    if value is None or value == "None":
        return "none"
    if value is True or value == "true":
        return "on"
    if value is False or value == "false":
        return "off"
    return str(value)


def options_block() -> str:
    header = "| option | type | default |\n|---|---|---|"
    rows = [f"| `{name}` | {type_name} | {shown} |" for name, type_name, shown in option_defaults()]
    return "\n".join([header, *rows])


def presets_block() -> str:
    columns = [
        ("indent", "indent_size"),
        ("inline arrays", "max_inline_array_size"),
        ("inline objects", "max_inline_object_size"),
        ("line width", "max_line_width"),
        ("sort keys", "sort_keys"),
        ("align values", "align_values"),
        ("colon spacing", "colon_spacing"),
        ("bracket spacing", "bracket_spacing"),
        ("escape unicode", "escape_unicode"),
        ("intelligent wrap", "intelligent_wrapping"),
    ]
    header = "| preset | " + " | ".join(label for label, _ in columns) + " |"
    sep = "|" + "---|" * (len(columns) + 1)
    lines = [header, sep]
    for name, values in preset_rows():
        cells = []
        for _, field in columns:
            value = values.get(field)
            if field == "max_line_width" and value == 0:
                cells.append("0 (no limit)")
            else:
                cells.append(bool_cell(value))
        lines.append(f"| {name} | " + " | ".join(cells) + " |")
    return "\n".join(lines)


# ------------------------------------------------------------------ examples
# The example outputs are produced BY THE FORMATTER, not typed by hand: the doc's sample
# lives in docs/formatting-sample.json and every output below is a real run of ./jsom.
SAMPLE = ROOT / "docs/formatting-sample.json"

EXAMPLES: dict[str, list[str] | None] = {
    "sample": None,  # the input document itself
    "example-compact": ["--preset=compact"],
    "example-pretty": ["--preset=pretty"],
    "example-config": ["--preset=config"],
    "example-api": ["--preset=api"],
    "example-debug": ["--preset=debug"],
    "example-indent": ["--preset=compact", "--indent=4"],
    "example-ultra-compact": ["--preset=pretty", "--inline-arrays=50", "--inline-objects=1"],
    "example-verbose": ["--preset=pretty", "--inline-arrays=0", "--inline-objects=0"],
}


def formatter_binary() -> Path | None:
    for candidate in (
        os.environ.get("CI_BUILD_DIR"),
        os.environ.get("JSOM_BUILD_DIR"),
        str(ROOT / "build"),
    ):
        if candidate and (Path(candidate) / "jsom").exists():
            return Path(candidate) / "jsom"
    return None


def example_block(name: str) -> str:
    flags = EXAMPLES[name]
    if flags is None:
        body = SAMPLE.read_text().rstrip("\n")
    else:
        binary = formatter_binary()
        if binary is None:
            raise RuntimeError(
                "docs examples need a built ./jsom (or CI_BUILD_DIR); build first"
            )
        out = subprocess.run(
            [str(binary), "format", *flags, str(SAMPLE)],
            capture_output=True,
            text=True,
        )
        if out.returncode != 0:
            raise RuntimeError(f"{name}: formatter failed: {out.stderr.strip()[:80]}")
        body = out.stdout.rstrip("\n")
    return f"```json\n{body}\n```"


BLOCKS: dict[str, object] = {
    "options": options_block,
    "presets": presets_block,
    **{name: (lambda n=name: example_block(n)) for name in EXAMPLES},
}
BEGIN = "<!-- BEGIN GENERATED: {name} — tools/check_docs.py --write -->"
END = "<!-- END GENERATED: {name} -->"


def check_blocks(write: bool) -> list[str]:
    text = DOC.read_text()
    problems = []
    for name, build in BLOCKS.items():
        want = f"{BEGIN.format(name=name)}\n{build()}\n{END.format(name=name)}"
        pattern = re.compile(
            re.escape(BEGIN.format(name=name)) + r".*?" + re.escape(END.format(name=name)),
            re.S,
        )
        found = pattern.search(text)
        if not found:
            problems.append(f"FORMATTING.md: generated block '{name}' is missing")
            continue
        if found.group(0) != want:
            if write:
                text = text[: found.start()] + want + text[found.end() :]
                print(f"  wrote generated block '{name}'")
            else:
                problems.append(f"FORMATTING.md: generated block '{name}' is stale")
    if write:
        DOC.write_text(text)
    return problems


# re.S matters: CLAUDE.md wraps the code span across two lines, and a single-line pattern
# silently missed it (found by hand on 2026-09-21 after this check had "passed").
STAGE_LIST = re.compile(r"`(tree [^`]*?pristine[^`]*)`", re.S)


def check_stage_lists(write: bool) -> list[str]:
    """The stage list is written down in the docs and was stale twice (sibling sessions
    added stages). tools/ci.sh holds the one definition; the docs must quote it."""
    ci = (ROOT / "tools/ci.sh").read_text()
    m = re.search(r'CI_DEFAULT_STAGES=\$\{CI_DEFAULT_STAGES:-"([^"]+)"\}', ci)
    if not m:
        return ["tools/ci.sh: could not read CI_DEFAULT_STAGES"]
    default = m.group(1)
    problems = []
    for rel in ("README.md", "CLAUDE.md", "CODING_STANDARDS.md"):
        path = ROOT / rel
        if not path.exists():
            continue
        text = path.read_text()
        if write:
            replaced = STAGE_LIST.sub(
                lambda hit: f"`{default}`"
                if " ".join(hit.group(1).split()) != default
                else hit.group(0),
                text,
            )
            if replaced != text:
                path.write_text(replaced)
                print(f"  updated the stage list in {rel}")
            continue
        for hit in STAGE_LIST.finditer(text):
            if " ".join(hit.group(1).split()) != default:
                problems.append(
                    f"{rel}: stage list does not match tools/ci.sh "
                    f"({' '.join(hit.group(1).split())[:60]}...)"
                )
    return problems


def main() -> int:
    argv = sys.argv[1:]
    write = "--write" in argv
    verbose = "--verbose" in argv

    if write:
        check_blocks(write=True)
        check_stage_lists(write=True)
        print("regenerated; re-run without --write to verify")
        return 0
    problems = check_blocks(False)
    problems += check_stage_lists(False)
    problems += check_retired(verbose)
    problems += check_targets()

    if problems:
        print("docs check FAILED:")
        for line in problems:
            print(f"  {line}")
        print("\nFix the text, or regenerate with tools/check_docs.py --write")
        return 1
    print("docs check passed: no retired identifiers, generated blocks match the code")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
