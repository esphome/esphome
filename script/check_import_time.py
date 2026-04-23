#!/usr/bin/env python3
"""Regression check for `import esphome.__main__` cost.

Runs `python -m importtime_waterfall --har esphome.__main__` (which invokes
`-X importtime` in fresh subprocesses, best-of-N) and compares the root
cumulative import time against a checked-in budget
(`script/import_time_budget.json`).

The CLI pays this cost on every invocation before the requested command even
runs, so a regression here hurts every user.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
from typing import Any, TextIO

SCRIPT_DIR = Path(__file__).parent
BUDGET_PATH = SCRIPT_DIR / "import_time_budget.json"

TARGET_MODULE = "esphome.__main__"
DEFAULT_MARGIN_PCT = 15
OFFENDERS_TOP_N = 15


def run_waterfall(module: str) -> str:
    """Run `importtime_waterfall --har <module>` and return the HAR JSON text.

    `importtime_waterfall` itself runs the target in 6 fresh subprocesses
    under `-X importtime` and emits the HAR of the fastest run.
    """
    result = subprocess.run(
        [sys.executable, "-m", "importtime_waterfall", "--har", module],
        check=True,
        stdout=subprocess.PIPE,
        text=True,
    )
    return result.stdout


def measure(module: str, har_path: Path | None = None) -> dict[str, Any]:
    """Return the parsed HAR for importing `module`.

    When `har_path` is given, also write the raw HAR JSON to that path so
    callers can combine `--check` with `--har` without measuring twice.
    """
    har_text = run_waterfall(module)
    if har_path is not None:
        har_path.write_text(har_text)
    return json.loads(har_text)


def _entries(har: dict[str, Any]) -> list[dict[str, Any]]:
    return har["log"]["entries"]


def root_cumulative_us(har: dict[str, Any], module: str) -> int:
    """Return the cumulative import time (µs) of `module` from a HAR.

    The HAR `time` field is authored by importtime_waterfall using µs values
    fed through `timedelta(milliseconds=...)`, so the number read back is the
    original self/cumulative time in microseconds (labelled "ms" in HAR).
    """
    for entry in _entries(har):
        if entry["request"]["url"] == module:
            return entry["time"]
    raise RuntimeError(
        f"No HAR entry for {module!r}. Is it importable with "
        f"`python -c 'import {module}'`?"
    )


def top_offenders(har: dict[str, Any], n: int) -> list[tuple[str, int, int]]:
    """Return up to `n` (name, self_us, cumulative_us), ranked by self_us desc.

    A module imported from multiple places is counted once (first entry wins,
    matching importtime's own de-duplication).
    """
    seen: dict[str, tuple[int, int]] = {}
    for entry in _entries(har):
        name = entry["request"]["url"]
        if name in seen:
            continue
        self_us = entry["timings"]["receive"]
        cumulative_us = entry["time"]
        seen[name] = (self_us, cumulative_us)
    ranked = sorted(
        ((name, s, c) for name, (s, c) in seen.items()),
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

    har = measure(TARGET_MODULE, har_path=Path(args.har) if args.har else None)
    measured = root_cumulative_us(har, TARGET_MODULE)

    baseline = budget["cumulative_us"]
    margin_pct = budget.get("margin_pct", DEFAULT_MARGIN_PCT)
    ceiling = int(baseline * (1 + margin_pct / 100))

    summary = (
        f"measured   {TARGET_MODULE}: {_format_us(measured)} "
        f"(budget {_format_us(baseline)} + {margin_pct}% = {_format_us(ceiling)})"
    )
    passed = measured <= ceiling
    stream = sys.stdout if passed else sys.stderr

    if passed:
        print(summary)
    else:
        print(
            f"REGRESSION: `import {TARGET_MODULE}` took {_format_us(measured)}, "
            f"exceeding the budget of {_format_us(baseline)} + {margin_pct}% "
            f"({_format_us(ceiling)}).",
            file=stream,
        )

    print("\nTop import-time offenders (by self time):", file=stream)
    _print_offenders_table(top_offenders(har, OFFENDERS_TOP_N), stream)

    if not passed:
        print(
            "\nIf this regression is intentional, regenerate the budget with:\n"
            "  script/check_import_time.py --update\n"
            "Otherwise, consider making the new import lazy "
            "(import inside the function that uses it).",
            file=stream,
        )
        return 1
    return 0


def cmd_update(args: argparse.Namespace) -> int:
    har = measure(TARGET_MODULE, har_path=Path(args.har) if args.har else None)
    measured = root_cumulative_us(har, TARGET_MODULE)
    write_budget(measured, args.margin_pct)
    print(
        f"Wrote {BUDGET_PATH.name}: "
        f"{TARGET_MODULE}={_format_us(measured)} "
        f"(margin {args.margin_pct}%)"
    )
    return 0


def cmd_har_only(args: argparse.Namespace) -> int:
    Path(args.har).write_text(run_waterfall(TARGET_MODULE))
    print(f"Wrote waterfall HAR to {args.har}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--margin-pct",
        type=int,
        default=DEFAULT_MARGIN_PCT,
        help=(f"Margin over baseline for --update (default: {DEFAULT_MARGIN_PCT}%%)."),
    )
    parser.add_argument(
        "--har",
        metavar="PATH",
        help=(
            "Write a waterfall HAR file at PATH. Can be combined with "
            "--check or --update to reuse that run's measurement (avoids "
            "measuring twice)."
        ),
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--check", action="store_true", help="Fail if measured time exceeds budget."
    )
    mode.add_argument(
        "--update",
        action="store_true",
        help="Rewrite the budget from a fresh measurement.",
    )
    args = parser.parse_args()

    if args.check:
        return cmd_check(args)
    if args.update:
        return cmd_update(args)
    if args.har:
        return cmd_har_only(args)
    parser.error("Specify at least one of --check, --update, or --har PATH.")
    return 2  # unreachable; parser.error exits. Here to satisfy ruff RET503.


if __name__ == "__main__":
    sys.exit(main())
