#!/usr/bin/env python3
"""Merge CI junit output into tests/integration/integration_test_durations.json.

The integration-tests CI job uploads one junit XML artifact per bucket on
full matrix dev runs. Download a run's artifacts and merge the per file
durations into the recording used by script/determine-jobs.py:

    gh run download <run-id> --repo esphome/esphome -p "junit-integration-*" -D /tmp/junit
    script/update_integration_test_durations.py /tmp/junit

Missing files keep their previous recording and deleted files drop out; a
run covering under 90% of the test files aborts unless --allow-partial.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from helpers import (
    INTEGRATION_TEST_DURATIONS_FILE,
    INTEGRATION_TESTS_PATH,
    all_integration_test_files,
    load_integration_durations,
    root_path,
)

DURATIONS_FILE = Path(root_path) / INTEGRATION_TEST_DURATIONS_FILE
MIN_COVERAGE = 0.9
# Exit code for the expected "run covers too few files" refusal, so the
# refresh workflow can move on to the next candidate run
EXIT_LOW_COVERAGE = 3


def collect_durations(junit_dir: Path, known_files: set[str]) -> dict[str, float]:
    """Sum junit testcase times per integration test file, in seconds."""
    durations: defaultdict[str, float] = defaultdict(float)
    unmatched = 0
    xml_files = sorted(junit_dir.rglob("*.xml"))
    if not xml_files:
        raise SystemExit(f"no junit XML files found under {junit_dir}")
    for xml_file in xml_files:
        for testcase in ET.parse(xml_file).getroot().iter("testcase"):
            # Skipped/errored testcases carry time="0"; recording them would
            # overwrite a good previous duration
            if any(
                testcase.find(tag) is not None
                for tag in ("skipped", "error", "failure")
            ):
                continue
            # classname is the dotted module plus any test class, e.g.
            # tests.integration.test_x or tests.integration.test_x.TestFoo
            parts = testcase.get("classname", "").split(".")
            if parts[:2] != ["tests", "integration"] or len(parts) < 3:
                unmatched += 1
                continue
            path = f"{INTEGRATION_TESTS_PATH}{parts[2]}.py"
            if path not in known_files:
                print(f"skipping unknown test module {path}", file=sys.stderr)
                continue
            durations[path] += float(testcase.get("time", "0"))
    if unmatched:
        # A junit naming change would otherwise shrink the recording silently
        raise SystemExit(
            f"{unmatched} testcases with unexpected classnames; the junit layout changed"
        )
    # An all-skipped file totals 0.0; let the merge keep its previous entry
    return {k: v for k, v in durations.items() if v > 0}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "junit_dir", type=Path, help="directory containing downloaded junit XML files"
    )
    parser.add_argument(
        "--allow-partial",
        action="store_true",
        help="merge a run covering under 90%% of the test files",
    )
    args = parser.parse_args()

    on_disk = set(all_integration_test_files())
    if not on_disk:
        raise SystemExit("no integration test files found; wrong checkout root?")
    collected = collect_durations(args.junit_dir, on_disk)
    coverage = len(collected.keys() & on_disk) / len(on_disk)
    if coverage < MIN_COVERAGE and not args.allow_partial:
        print(
            f"artifacts cover only {coverage:.0%} of {len(on_disk)} test files; "
            "use a full matrix run or pass --allow-partial to merge anyway",
            file=sys.stderr,
        )
        return EXIT_LOW_COVERAGE

    # Validated load: a bad previous entry cannot survive the round trip, and
    # an unreadable file aborts rather than being overwritten
    previous = load_integration_durations()
    if DURATIONS_FILE.is_file() and not previous:
        raise SystemExit(f"{DURATIONS_FILE} is unreadable; refusing to overwrite it")
    # New recordings win, absent files keep theirs, deleted files drop out
    merged = {
        path: collected.get(path, previous.get(path))
        for path in sorted(on_disk)
        if path in collected or path in previous
    }
    DURATIONS_FILE.write_text(
        json.dumps({k: round(v, 2) for k, v in merged.items()}, indent=2) + "\n"
    )
    print(f"wrote {len(merged)} entries to {DURATIONS_FILE} ({coverage:.0%} fresh)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
