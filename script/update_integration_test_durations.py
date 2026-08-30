#!/usr/bin/env python3
"""Regenerate tests/integration/integration_test_durations.json from CI junit output.

The integration-tests CI job uploads one junit XML artifact per bucket. Download
them from a run that executed the full matrix (e.g. a dev push, or a PR with the
`ci-run-all` label) and aggregate the per-file wall durations used by the
duration-weighted bucketing in script/determine-jobs.py:

    gh run download <run-id> --repo esphome/esphome -p "junit-integration-*" -D /tmp/junit
    script/update_integration_test_durations.py /tmp/junit
"""

from __future__ import annotations

import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from helpers import INTEGRATION_TEST_DURATIONS_FILE, root_path

DURATIONS_FILE = Path(root_path) / INTEGRATION_TEST_DURATIONS_FILE


def collect_durations(junit_dir: Path) -> dict[str, float]:
    """Sum junit testcase times per integration test file, in seconds."""
    durations: defaultdict[str, float] = defaultdict(float)
    xml_files = sorted(junit_dir.rglob("*.xml"))
    if not xml_files:
        raise SystemExit(f"no junit XML files found under {junit_dir}")
    for xml_file in xml_files:
        for testcase in ET.parse(xml_file).getroot().iter("testcase"):
            # classname is the dotted module, e.g. tests.integration.test_api
            classname = testcase.get("classname", "")
            if not classname.startswith("tests.integration."):
                continue
            path = classname.replace(".", "/") + ".py"
            durations[path] += float(testcase.get("time", "0"))
    return dict(durations)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "junit_dir", type=Path, help="directory containing downloaded junit XML files"
    )
    args = parser.parse_args()

    durations = collect_durations(args.junit_dir)
    DURATIONS_FILE.write_text(
        json.dumps({k: round(v, 2) for k, v in sorted(durations.items())}, indent=2)
        + "\n"
    )
    print(f"wrote {len(durations)} entries to {DURATIONS_FILE}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
