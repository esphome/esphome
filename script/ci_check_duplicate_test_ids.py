#!/usr/bin/env python3
"""Fail when two component test fixtures define the same id with different content.

Component tests are merged and built in groups in CI (see
``script/merge_component_configs.py``). When two components declare the same id
under the same section but with different content, the merge silently keeps the
first and drops the rest, which can make a cross-reference resolve to an
incompatible entity (this is what broke the i2s_audio speaker tests). The merge
now raises on such a collision, but only when the two components land in the same
group. This script is the complete, batch-independent guard: it scans every
component's ``test.<platform>.yaml`` per platform and reports any id that is
defined by more than one component with differing content.

Ids that are intentionally shared across components (e.g. a singleton
``sntp_time`` clock) are listed in ``INTENTIONALLY_SHARED_IDS`` and skipped.
"""

from __future__ import annotations

from collections import defaultdict
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent.parent))

from script.merge_component_configs import (  # noqa: E402
    INTENTIONALLY_SHARED_IDS,
    load_yaml_file,
)

TESTS_DIR = Path("tests/components")


def _normalize(value: object) -> object:
    """Return a hashable, order-independent representation for comparison."""
    if isinstance(value, dict):
        return tuple(sorted((str(k), _normalize(v)) for k, v in value.items()))
    if isinstance(value, (list, tuple)):
        return tuple(_normalize(v) for v in value)
    # Scalars (and ESPHome tag objects like !lambda) compare by their text form
    return str(value)


def _collect_ids(
    data: object, section: str, out: dict[tuple[str, str], object]
) -> None:
    """Walk a parsed config and record (section, id) -> normalized content."""
    if isinstance(data, dict):
        for key, value in data.items():
            if isinstance(value, list):
                for item in value:
                    if isinstance(item, dict) and "id" in item:
                        out[(key, str(item["id"]))] = _normalize(item)
                    _collect_ids(item, key, out)
            else:
                _collect_ids(value, key, out)
    elif isinstance(data, list):
        for item in data:
            _collect_ids(item, section, out)


def _discover_platforms() -> set[str]:
    platforms: set[str] = set()
    for test_file in TESTS_DIR.glob("*/test.*.yaml"):
        # test.<platform>.yaml -> platform is the middle dotted part
        parts = test_file.name.split(".")
        if len(parts) == 3:
            platforms.add(parts[1])
    return platforms


def main() -> int:
    conflicts: list[str] = []
    for platform in sorted(_discover_platforms()):
        # (section, id) -> {normalized_content: [components]}
        by_id: dict[tuple[str, str], dict[object, list[str]]] = defaultdict(
            lambda: defaultdict(list)
        )
        for comp_dir in sorted(TESTS_DIR.iterdir()):
            if not comp_dir.is_dir():
                continue
            test_file = comp_dir / f"test.{platform}.yaml"
            if not test_file.exists():
                continue
            try:
                data = load_yaml_file(test_file)
            except Exception as err:  # noqa: BLE001
                print(f"WARNING: could not parse {test_file}: {err}", file=sys.stderr)
                continue
            ids: dict[tuple[str, str], object] = {}
            _collect_ids(data, "", ids)
            for (section, id_), content in ids.items():
                if id_ in INTENTIONALLY_SHARED_IDS:
                    continue
                by_id[(section, id_)][content].append(comp_dir.name)

        for (section, id_), variants in sorted(by_id.items()):
            if len(variants) < 2:
                continue
            components = sorted({c for comps in variants.values() for c in comps})
            conflicts.append(
                f"[{platform}] id '{id_}' under '{section}' is defined "
                f"differently by: {', '.join(components)}"
            )

    if conflicts:
        print("Conflicting test component ids found:\n")
        for line in conflicts:
            print(f"  - {line}")
        print(
            "\nGive each component a unique id (e.g. '<component>_<id>'), or add the "
            "id to INTENTIONALLY_SHARED_IDS in script/merge_component_configs.py if "
            "it is a deliberately shared singleton."
        )
        return 1

    print("No conflicting test component ids found.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
