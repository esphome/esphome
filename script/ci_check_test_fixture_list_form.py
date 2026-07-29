#!/usr/bin/env python3
"""Fail when a test fixture writes a platform-list domain as a single dict.

Component tests are merged and built in groups in CI (see
``script/merge_component_configs.py``). ESPHome's ``merge_config`` concatenates
two lists, but when one side is a dict it replaces the other side wholesale
(``esphome/config_helpers.py``). A domain such as ``one_wire:`` or ``ota:``
written in single-dict form therefore deletes every entry other components
contributed to that domain before it in the merge, and is itself deleted by any
list that merges after it. The resulting failure only appears when the affected
components land in the same group -- usually a full component matrix run on an
unrelated PR long after the fixture was written (this is what broke the
dallas_temp tests when ds2484 was added, see #17868).

This guard scans every fixture under ``tests/components/`` and rejects any
top-level domain written as a dict with a ``platform`` key. Such a domain is by
definition a platform list (single-dict form is only user-config sugar), so the
fix is always to write it as a one-element list:

    one_wire:
      - platform: gpio
        pin: 4
"""

from __future__ import annotations

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent.parent))

from esphome.core import EsphomeError  # noqa: E402
from script.analyze_component_buses import ISOLATED_COMPONENTS  # noqa: E402
from script.merge_component_configs import load_yaml_file  # noqa: E402

# Resolved relative to this file (not the CWD) so the scan cannot silently cover
# nothing when run from a different directory.
ROOT_DIR = Path(__file__).resolve().parent.parent
TESTS_DIR = ROOT_DIR / "tests" / "components"


def main() -> int:
    offenders: list[str] = []
    parse_errors: list[str] = []
    fixtures_scanned = 0

    for fixture in sorted(TESTS_DIR.glob("*/*.yaml")):
        # Isolated components are never merged with others, so dict form
        # cannot clobber anyone there.
        if fixture.parent.name in ISOLATED_COMPONENTS:
            continue
        try:
            data = load_yaml_file(fixture)
        except EsphomeError as err:
            parse_errors.append(f"{fixture.relative_to(ROOT_DIR)}: {err}")
            continue
        fixtures_scanned += 1
        if not isinstance(data, dict):
            continue
        for key, value in data.items():
            if isinstance(value, dict) and "platform" in value:
                offenders.append(f"{fixture.relative_to(ROOT_DIR)}: '{key}:'")

    if offenders:
        print("Test fixtures with platform domains in single-dict form:\n")
        for line in offenders:
            print(f"  - {line}")
        print(
            "\nWrite the domain as a one-element list ('- platform: ...') so "
            "grouped CI builds can merge it with other components' entries; "
            "in dict form it replaces or is replaced by their lists wholesale."
        )

    if parse_errors:
        # A fixture we could not parse was never scanned, so the run is not a
        # clean pass even if no offenders were found among the rest.
        print(
            f"\n{len(parse_errors)} test fixture(s) could not be parsed and "
            "were not checked:"
        )
        for line in parse_errors:
            print(f"  - {line}")

    if fixtures_scanned == 0:
        # A scan that covered nothing is a false green -- the whole point of the
        # guard is defeated. Fail loudly (wrong working directory or layout change).
        print(
            f"\nERROR: scanned 0 test fixtures under {TESTS_DIR}; "
            "the guard covered nothing.",
            file=sys.stderr,
        )

    if offenders or parse_errors or fixtures_scanned == 0:
        return 1

    print(
        f"No single-dict platform domains found ({fixtures_scanned} fixtures scanned)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
