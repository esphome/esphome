#!/usr/bin/env python3
"""Fail when two component test fixtures define the same id with different content.

Component tests are merged and built in groups in CI (see
``script/merge_component_configs.py``). When two components declare the same id
under the same section but with different content, the merge keeps the first and
drops the rest, which can make a cross-reference resolve to an incompatible
entity (this is what broke the i2s_audio speaker tests). That only surfaces when
the two components happen to land in the same group, often in an unrelated PR
long after the duplicate was written.

This script is the complete, batch-independent guard: it scans every component's
``test.<platform>.yaml`` per platform and reports any id that is defined by more
than one component with differing content, so a collision fails the PR that
introduces it and names the exact id and components.

To stay byte-for-byte consistent with what the merge actually does (so the guard
never disagrees with the build), it reuses the merge's own helpers:

* ``prefix_substitutions_in_dict`` -- the merge prefixes every component's
  substitution references with the component name before deduplicating, so e.g.
  ``pin: ${pin}`` in two components becomes ``${a_pin}`` and ``${b_pin}`` and
  conflicts. We apply the same prefixing; otherwise a shared id whose only
  difference is a substitution looks identical here but conflicts at merge time.
* ``deduplicate_by_id`` -- the actual merge comparison (including the
  ``INTENTIONALLY_SHARED_IDS`` allowlist for deliberately shared singletons such
  as ``sntp_time``). We feed each shared id's prefixed items straight through it
  and treat a raised ``ValueError`` as a conflict, so this check and the merge
  can never diverge.

``packages:`` are left as opaque ``!include`` objects by the loader -- exactly as
the merge sees them at dedup time -- so package-provided bus ids (``i2c_bus`` ...)
are not compared here, matching the merge, which re-adds those packages once.
"""

from __future__ import annotations

from collections import defaultdict
from collections.abc import Iterator
from dataclasses import dataclass, field
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent.parent))

from esphome.core import EsphomeError  # noqa: E402
from script.merge_component_configs import (  # noqa: E402
    deduplicate_by_id,
    load_yaml_file,
    prepare_component_body,
)

# Resolved relative to this file (not the CWD) so the scan cannot silently cover
# nothing when run from a different directory.
TESTS_DIR = Path(__file__).resolve().parent.parent / "tests" / "components"


def _collect_ids(
    data: object,
    path: tuple[str, ...],
    out: dict[tuple[tuple[str, ...], object], object],
) -> None:
    """Record (dict_path, id) -> item for id-bearing items in dict-reachable lists.

    Keyed by the full dict path (not just the immediate key) so items under
    different paths that happen to share a list key name are never compared. Only
    lists reached purely through dict keys are recorded: once the merge
    concatenates a list, items from different components live in separate elements,
    so anything deeper is never compared across components (matching how
    ``merge_config`` combines bodies). Ids keep their original type so ``5`` and
    ``"5"`` stay distinct, exactly as ``deduplicate_by_id`` treats them; an
    unhashable id (rare) falls back to its ``repr`` so it can still be grouped.
    """
    if not isinstance(data, dict):
        return
    for key, value in data.items():
        new_path = path + (key,)
        if isinstance(value, list):
            for item in value:
                if isinstance(item, dict) and "id" in item:
                    item_id = item["id"]
                    try:
                        hash(item_id)
                    except TypeError:
                        item_id = repr(item_id)
                    out[(new_path, item_id)] = item
        elif isinstance(value, dict):
            _collect_ids(value, new_path, out)


def _discover_platforms() -> set[str]:
    platforms: set[str] = set()
    for test_file in TESTS_DIR.glob("*/test.*.yaml"):
        # test.<platform>.yaml -> platform is the middle dotted part
        parts = test_file.name.split(".")
        if len(parts) == 3:
            platforms.add(parts[1])
    return platforms


def _load_components(
    platform: str, parse_errors: list[str]
) -> Iterator[tuple[str, object]]:
    """Yield (component, prefixed config) for each component testing this platform.

    Each body is prepared with ``prepare_component_body`` (the same helper the
    merge uses: it expands component-specific package includes and prefixes
    substitutions), so the comparison sees what the build merges. Fixtures that
    fail to parse are recorded in ``parse_errors`` so the run can fail rather than
    silently skip them.
    """
    for comp_dir in sorted(TESTS_DIR.iterdir()):
        test_file = comp_dir / f"test.{platform}.yaml"
        if not comp_dir.is_dir() or not test_file.exists():
            continue
        try:
            data = load_yaml_file(test_file)
        except EsphomeError as err:
            parse_errors.append(str(test_file))
            print(f"ERROR: could not parse {test_file}: {err}", file=sys.stderr)
            continue
        yield comp_dir.name, prepare_component_body(data, comp_dir.name, comp_dir)


@dataclass
class ScanResult:
    """Outcome of a scan. A caller cannot observe a clean result while files were
    skipped or nothing was scanned -- all three fields are reported together."""

    conflicts: list[str] = field(default_factory=list)
    parse_errors: list[str] = field(default_factory=list)
    components_scanned: int = 0


def scan() -> ScanResult:
    """Scan every component's base test fixture and report cross-component id conflicts.

    Only base ``test.<platform>.yaml`` fixtures are scanned because only those are
    combined by ``merge_component_configs`` in grouped CI builds; variant
    (``test-*.yaml``) fixtures are built individually and never cross-merged.
    """
    result = ScanResult()
    for platform in sorted(_discover_platforms()):
        # (dict_path, id) -> {component: prefixed_item}
        groups: dict[tuple[tuple[str, ...], object], dict[str, object]] = defaultdict(
            dict
        )
        for component, data in _load_components(platform, result.parse_errors):
            result.components_scanned += 1
            collected: dict[tuple[tuple[str, ...], object], object] = {}
            _collect_ids(data, (), collected)
            for key, item in collected.items():
                groups[key][component] = item

        for (path, id_), by_component in sorted(
            groups.items(), key=lambda kv: (kv[0][0], str(kv[0][1]))
        ):
            if len(by_component) < 2:
                continue
            # Delegate the decision to the merge's own deduplication so this guard
            # can never disagree with what the build does.
            try:
                deduplicate_by_id({path[-1]: list(by_component.values())})
            except ValueError:
                result.conflicts.append(
                    f"[{platform}] id '{id_}' under '{'.'.join(path)}' is defined "
                    f"differently by: {', '.join(sorted(by_component))}"
                )
    return result


def main() -> int:
    result = scan()
    if result.conflicts:
        print("Conflicting test component ids found:\n")
        for line in result.conflicts:
            print(f"  - {line}")
        print(
            "\nGive each component a unique id (e.g. '<component>_<id>'), or add the "
            "id to INTENTIONALLY_SHARED_IDS in script/merge_component_configs.py if "
            "it is a deliberately shared singleton."
        )

    if result.parse_errors:
        # A fixture we could not parse was never scanned, so the run is not a
        # clean pass even if no conflicts were found among the rest.
        print(
            f"\n{len(result.parse_errors)} test fixture(s) could not be parsed and "
            "were not checked:"
        )
        for path in result.parse_errors:
            print(f"  - {path}")

    if result.components_scanned == 0:
        # A scan that covered nothing is a false green -- the whole point of the
        # guard is defeated. Fail loudly (wrong working directory or layout change).
        print(
            f"\nERROR: scanned 0 component test fixtures under {TESTS_DIR}; "
            "the guard covered nothing.",
            file=sys.stderr,
        )

    if result.conflicts or result.parse_errors or result.components_scanned == 0:
        return 1

    print(
        f"No conflicting test component ids found "
        f"({result.components_scanned} fixtures scanned)."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
