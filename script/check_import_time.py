#!/usr/bin/env python3
"""Regression check for `import esphome.__main__` cost.

Runs `python -X importtime -c "import esphome.__main__"` in fresh subprocesses
and compares the root cumulative import time against a checked-in budget
(`script/import_time_budget.json`).

The CLI pays this cost on every invocation before the requested command even
runs, so a regression here hurts every user. Pair this with
`python -m importtime_waterfall --har` for human-readable waterfalls.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
from typing import Any, TextIO

SCRIPT_DIR = Path(__file__).parent
BUDGET_PATH = SCRIPT_DIR / "import_time_budget.json"

TARGET_MODULE = "esphome.__main__"
DEFAULT_RUNS = 3
DEFAULT_MARGIN_PCT = 15
OFFENDERS_TOP_N = 15
IMPORT_TIME_PREFIX = "import time:"


def _run_importtime(module: str) -> tuple[float, str]:
    """Run `python -X importtime -c 'import <module>'` once.

    Returns (wall_seconds, stderr_text).
    """
    before = time.monotonic()
    result = subprocess.run(
        [sys.executable, "-X", "importtime", "-c", f"import {module}"],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    return time.monotonic() - before, result.stderr


def measure(module: str, runs: int) -> str:
    """Run `module` import `runs` times; return the fastest run's stderr."""
    best_wall, best_stderr = _run_importtime(module)
    for _ in range(runs - 1):
        wall, stderr = _run_importtime(module)
        if wall < best_wall:
            best_wall, best_stderr = wall, stderr
    return best_stderr


def parse_trace(stderr: str) -> list[tuple[int, int, int, str]]:
    """Parse `-X importtime` stderr into (depth, self_us, cumulative_us, name)."""
    entries: list[tuple[int, int, int, str]] = []
    for line in stderr.splitlines():
        if not line.startswith(IMPORT_TIME_PREFIX):
            continue
        body = line[len(IMPORT_TIME_PREFIX) :]
        parts = body.split("|")
        if len(parts) != 3:
            continue
        try:
            self_us = int(parts[0].strip())
            cumulative_us = int(parts[1].strip())
        except ValueError:
            continue  # header row
        name_field = parts[2].rstrip("\n")
        name = name_field.lstrip(" ")
        depth = (len(name_field) - len(name)) // 2
        entries.append((depth, self_us, cumulative_us, name))
    return entries


def root_cumulative_us(entries: list[tuple[int, int, int, str]], module: str) -> int:
    """Return the cumulative import time of `module` (the root probe)."""
    for depth, _self_us, cumulative_us, name in reversed(entries):
        if depth == 0 and name == module:
            return cumulative_us
    raise RuntimeError(
        f"Did not find a root-level trace entry for {module!r}. Is it importable?"
    )


def top_offenders(
    entries: list[tuple[int, int, int, str]], n: int
) -> list[tuple[str, int, int]]:
    """Return up to `n` modules with the highest self-time, deduped by name.

    Returns list of (name, self_us, cumulative_us). A module imported from
    multiple places is counted once, at its deepest-stacktrace occurrence
    (same as what the user sees on the `--graph` output).
    """
    seen: dict[str, tuple[int, int]] = {}
    for _depth, self_us, cumulative_us, name in entries:
        if name in seen:
            continue
        seen[name] = (self_us, cumulative_us)
    ranked = sorted(
        ((name, self_us, cum_us) for name, (self_us, cum_us) in seen.items()),
        key=lambda row: row[1],
        reverse=True,
    )
    return ranked[:n]


def read_budget() -> dict[str, Any]:
    if not BUDGET_PATH.exists():
        return {}
    with BUDGET_PATH.open() as f:
        return json.load(f)


def write_budget(cumulative_us: int, margin_pct: int) -> None:
    payload = {
        "target_module": TARGET_MODULE,
        "margin_pct": margin_pct,
        "cumulative_us": cumulative_us,
    }
    with BUDGET_PATH.open("w") as f:
        json.dump(payload, f, indent=2)
        f.write("\n")


def _format_us(us: int) -> str:
    if us >= 1000:
        return f"{us / 1000:.1f}ms"
    return f"{us}us"


def _print_offenders_table(
    offenders: list[tuple[str, int, int]], stream: TextIO
) -> None:
    name_w = max(len(name) for name, _, _ in offenders)
    print(f"\n{'module':<{name_w}}  {'self':>10}  {'cumulative':>12}", file=stream)
    print(f"{'-' * name_w}  {'-' * 10}  {'-' * 12}", file=stream)
    for name, self_us, cum_us in offenders:
        print(
            f"{name:<{name_w}}  {_format_us(self_us):>10}  {_format_us(cum_us):>12}",
            file=stream,
        )


def cmd_check(args: argparse.Namespace) -> int:
    budget = read_budget()
    if not budget:
        print(
            f"ERROR: {BUDGET_PATH.name} missing. Run with --update first.",
            file=sys.stderr,
        )
        return 2

    stderr = measure(TARGET_MODULE, args.runs)
    entries = parse_trace(stderr)
    measured = root_cumulative_us(entries, TARGET_MODULE)

    baseline = budget["cumulative_us"]
    margin_pct = budget.get("margin_pct", DEFAULT_MARGIN_PCT)
    ceiling = int(baseline * (1 + margin_pct / 100))

    summary = (
        f"measured   {TARGET_MODULE}: {_format_us(measured)} "
        f"(budget {_format_us(baseline)} + {margin_pct}% = {_format_us(ceiling)})"
    )

    if measured <= ceiling:
        print(summary)
        return 0

    print(
        f"REGRESSION: `import {TARGET_MODULE}` took {_format_us(measured)}, "
        f"exceeding the budget of {_format_us(baseline)} + {margin_pct}% "
        f"({_format_us(ceiling)}).\n"
        f"Top import-time offenders (by self time):",
        file=sys.stderr,
    )
    _print_offenders_table(top_offenders(entries, OFFENDERS_TOP_N), sys.stderr)
    print(
        "\nIf this regression is intentional, regenerate the budget with:\n"
        "  script/check_import_time.py --update\n"
        "Otherwise, consider making the new import lazy "
        "(import inside the function that uses it).",
        file=sys.stderr,
    )
    return 1


def cmd_update(args: argparse.Namespace) -> int:
    stderr = measure(TARGET_MODULE, args.runs)
    entries = parse_trace(stderr)
    measured = root_cumulative_us(entries, TARGET_MODULE)
    write_budget(measured, args.margin_pct)
    print(
        f"Wrote {BUDGET_PATH.name}: "
        f"{TARGET_MODULE}={_format_us(measured)} "
        f"(margin {args.margin_pct}%)"
    )
    return 0


def cmd_har(args: argparse.Namespace) -> int:
    out_path = Path(args.har)
    result = subprocess.run(
        [sys.executable, "-m", "importtime_waterfall", "--har", TARGET_MODULE],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    out_path.write_text(result.stdout)
    print(f"Wrote waterfall HAR to {out_path}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--runs",
        type=int,
        default=DEFAULT_RUNS,
        help=f"Number of measurement runs (default: {DEFAULT_RUNS}, best-of-N).",
    )
    parser.add_argument(
        "--margin-pct",
        type=int,
        default=DEFAULT_MARGIN_PCT,
        help=(f"Margin over baseline for --update (default: {DEFAULT_MARGIN_PCT}%%)."),
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--check", action="store_true", help="Fail if measured time exceeds budget."
    )
    mode.add_argument(
        "--update",
        action="store_true",
        help="Rewrite the budget from a fresh measurement.",
    )
    mode.add_argument(
        "--har",
        metavar="PATH",
        help="Write a waterfall HAR file via `importtime_waterfall --har`.",
    )
    args = parser.parse_args()

    if args.check:
        return cmd_check(args)
    if args.update:
        return cmd_update(args)
    if args.har:
        return cmd_har(args)
    return 2


if __name__ == "__main__":
    sys.exit(main())
