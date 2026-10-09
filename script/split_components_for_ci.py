#!/usr/bin/env python3
"""Split components into batches with intelligent grouping.

This script analyzes components to identify which ones share common bus configurations
and intelligently groups them into batches to maximize the efficiency of the
component grouping system in CI.

Components with the same bus signature are placed in the same batch whenever possible,
allowing the test_build_components.py script to merge them into single builds.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from collections.abc import Set as AbstractSet
import contextlib
from dataclasses import dataclass, field
import functools
import json
import math
from pathlib import Path
import sys
from typing import Any, NamedTuple

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from esphome import yaml_util
from script.analyze_component_buses import (
    ISOLATED_COMPONENTS,
    ISOLATED_SIGNATURE_PREFIX,
    NO_BUSES_SIGNATURE,
    analyze_all_components,
    create_grouping_signature,
    merge_compatible_bus_groups,
)
from script.helpers import (
    _extract_components_from_yaml,
    get_component_test_files,
    parse_test_filename,
    split_conflicting_groups,
)

# Estimated CI seconds for one build per test platform, fitted to the batch
# job logs of a full run (every component, PR #20389). Absolute accuracy does
# not matter, only the ratios between platforms. A full run takes over an
# hour, so recording real timings is not practical.
PLATFORM_BUILD_SECONDS = {
    "host": 10,
    "esp8266-ard": 30,
    "rtl87xx-ard": 40,
    "esp32-c3-idf": 40,
    "esp32-s2-idf": 40,
    "rp2350-ard": 40,
    "rp2040-ard": 45,
    "esp32-idf": 55,
    "esp32-p4-idf": 55,
    "ln882x-ard": 55,
    "esp32-s3-idf": 60,
    "bk72xx-ard": 70,
    "esp32-c6-idf": 70,
}
NRF52_BUILD_SECONDS = 50
# Arduino on any ESP32 chip builds the IDF plus the Arduino core on top
ESP32_ARDUINO_BUILD_SECONDS = 90
# The remaining ESP32 chips on ESP-IDF
DEFAULT_BUILD_SECONDS = 50
# Each extra component merged into a grouped build makes it larger
GROUPED_COMPONENT_SECONDS = 3
# Components that pull in a large library the build compiles from source
# (TensorFlow Lite Micro, FastLED, LVGL, the HTTP server), added to every
# build whose test config enables them. Fitted the same way as the platforms.
HEAVY_COMPONENT_SECONDS: dict[str, int] = {
    "micro_wake_word": 200,
    "fastled_clockless": 90,
    "fastled_spi": 90,
    "wled": 60,
    "lvgl": 50,
    "web_server": 50,
    "online_image": 45,
    "prometheus": 35,
    "bluetooth_proxy": 30,
}
# Estimated build seconds per CI runner; sets how many runners are used.
# Approximate: the runner count charges each grouped build once, but a large
# group spreads over several runners, which each pay for that build.
TARGET_BATCH_SECONDS = 600

# Platform used for batching (platform-agnostic batching)
# Batches are split across CI runners and each runner tests all platforms
ALL_PLATFORMS = "all"


def has_test_files(component_name: str, tests_dir: Path) -> bool:
    """Check if a component has test files.

    Validate files (validate.*.yaml) count -- a component with only config-only
    test files still needs a CI runner for schema validation.

    Args:
        component_name: Name of the component
        tests_dir: Path to tests/components directory (unused, kept for compatibility)

    Returns:
        True if the component has test.*.yaml, test-*.yaml, or validate.*.yaml files
    """
    return bool(
        get_component_test_files(
            component_name, all_variants=True, include_validate=True
        )
    )


def build_seconds(platform: str) -> int:
    """Return the estimated CI seconds for one build on a test platform."""
    if (seconds := PLATFORM_BUILD_SECONDS.get(platform)) is not None:
        return seconds
    if platform.startswith("nrf52"):
        return NRF52_BUILD_SECONDS
    if platform.startswith("esp32") and platform.endswith("-ard"):
        return ESP32_ARDUINO_BUILD_SECONDS
    return DEFAULT_BUILD_SECONDS


def _config_components(data: Any) -> set[str]:
    """Return the components a loaded test config and its ``packages:`` use."""
    if not isinstance(data, dict):
        return set()
    found = _extract_components_from_yaml(data)
    if isinstance(packages := data.get("packages"), dict):
        for package in packages.values():
            if isinstance(package, yaml_util.IncludeFile):
                found |= _file_components(
                    (package.parent_file.parent / package.file).resolve()
                )
            else:
                found |= _config_components(package)
    return found


@functools.cache
def _file_components(path: Path) -> frozenset[str]:
    """Return the components a config file uses; shared packages load once."""
    return frozenset(
        _config_components(yaml_util.load_yaml(path, track_document_range=False))
    )


def heavy_components(component: str, test_file: Path) -> frozenset[str]:
    """Return the heavy components a test compiles.

    A test counts as heavy when it belongs to a heavy component or when its
    config enables one, such as a display test that draws with lvgl.
    """
    heavy = {component} & HEAVY_COMPONENT_SECONDS.keys()
    # A config that does not load is left to the build to report
    with contextlib.suppress(Exception):
        heavy |= _file_components(test_file.resolve()) & HEAVY_COMPONENT_SECONDS.keys()
    return frozenset(heavy)


def heavy_seconds(heavy: AbstractSet[str]) -> int:
    """Return the library seconds a set of heavy components adds to a build."""
    return sum(HEAVY_COMPONENT_SECONDS[name] for name in heavy)


class _GroupedShare(NamedTuple):
    """A component's part in one (signature, platform) shared build."""

    # Seconds of its files on that platform when no other member shares it
    solo: int
    # Heavy components its base test brings into the shared build
    heavy: frozenset[str]


@dataclass
class _BatchItem:
    """A component and the builds it adds to the batch it lands in."""

    component: str
    # Builds that always run on their own (isolated tests, variants on a
    # platform without a base test)
    own_seconds: int
    grouped_builds: dict[tuple[str, str], _GroupedShare] = field(default_factory=dict)

    def standalone_seconds(self) -> int:
        return self.own_seconds + sum(
            share.solo for share in self.grouped_builds.values()
        )


def _grouped_build_seconds(platform: str, members: list[_GroupedShare]) -> int:
    """Return the seconds of one (signature, platform) build in a batch.

    A library two members both enable compiles once, so the union is charged.
    test_build_components only groups when two or more members share the
    build; a grouped member then skips its variants on that platform.
    """
    if len(members) <= 1:
        return sum(share.solo for share in members)
    return (
        build_seconds(platform)
        + GROUPED_COMPONENT_SECONDS * (len(members) - 1)
        + heavy_seconds(set().union(*(share.heavy for share in members)))
    )


def _make_item(
    tests_dir: Path, component: str, signature: str, is_isolated: bool
) -> _BatchItem:
    # Per platform: the seconds of its test files, and the base test's heavy
    # components; a platform without a base test cannot join a shared build
    seconds_by_platform: dict[str, int] = defaultdict(int)
    base_heavy: dict[str, frozenset[str]] = {}
    for test_file in (tests_dir / component).glob("test[.-]*.yaml"):
        test_name, platform = parse_test_filename(test_file)
        heavy = heavy_components(component, test_file)
        seconds_by_platform[platform] += build_seconds(platform) + heavy_seconds(heavy)
        if test_name == "test":
            base_heavy[platform] = heavy
    own_seconds = 0
    grouped: dict[tuple[str, str], _GroupedShare] = {}
    for platform, seconds in seconds_by_platform.items():
        if is_isolated or platform not in base_heavy:
            own_seconds += seconds
        else:
            grouped[(signature, platform)] = _GroupedShare(
                seconds, base_heavy[platform]
            )
    return _BatchItem(component, own_seconds, grouped)


class _Batch:
    """A CI runner's components and its estimated build seconds."""

    def __init__(self) -> None:
        self.components: list[str] = []
        self.seconds = 0
        self.grouped_builds: dict[tuple[str, str], list[_GroupedShare]] = defaultdict(
            list
        )

    def added_seconds(self, item: _BatchItem) -> int:
        """Return the seconds item would add; joining an existing build is cheap."""
        added = item.own_seconds
        for (signature, platform), share in item.grouped_builds.items():
            members = self.grouped_builds.get((signature, platform), [])
            added += _grouped_build_seconds(
                platform, [*members, share]
            ) - _grouped_build_seconds(platform, members)
        return added

    def add(self, item: _BatchItem) -> None:
        self.seconds += self.added_seconds(item)
        for build, share in item.grouped_builds.items():
            self.grouped_builds[build].append(share)
        self.components.append(item.component)


def balance_batches(items: list[_BatchItem], target_seconds: int) -> list[list[str]]:
    """Spread items over enough runners to stay near target_seconds each.

    The runner count comes from the total estimate with every grouped build
    counted once. Heaviest items go first, each to the runner that ends up
    lightest, so grouped components follow the builds they can join.
    """
    if not items:
        return []
    # One batch holding every item counts each grouped build once
    whole = _Batch()
    for item in items:
        whole.add(item)
    total = whole.seconds
    count = min(len(items), max(1, math.ceil(total / target_seconds)))
    batches = [_Batch() for _ in range(count)]
    for item in sorted(items, key=lambda i: (-i.standalone_seconds(), i.component)):
        min(batches, key=lambda b: (b.seconds + b.added_seconds(item), b.seconds)).add(
            item
        )
    return [batch.components for batch in batches if batch.components]


def create_intelligent_batches(
    components: list[str],
    tests_dir: Path,
    target_seconds: int = TARGET_BATCH_SECONDS,
    directly_changed: set[str] | None = None,
) -> tuple[list[list[str]], dict[tuple[str, str], list[str]]]:
    """Create batches optimized for component grouping.

    IMPORTANT: This function is called from both split_components_for_ci.py (standalone script)
    and determine-jobs.py (integrated into job determination). Be careful when refactoring
    to ensure changes work in both contexts.

    Args:
        components: List of component names to batch
        tests_dir: Path to tests/components directory
        target_seconds: Estimated build seconds per batch
        directly_changed: Set of directly changed components (for logging only)

    Returns:
        Tuple of (batches, signature_groups) where:
        - batches: List of component batches (lists of component names)
        - signature_groups: Dict mapping (platform, signature) to component lists
    """
    # Filter out components without test files
    # Platform components like 'climate' and 'climate_ir' don't have test files
    components_with_tests = [
        comp for comp in components if has_test_files(comp, tests_dir)
    ]

    # Log filtered components to stderr for debugging
    if len(components_with_tests) < len(components):
        filtered_out = set(components) - set(components_with_tests)
        print(
            f"Note: Filtered {len(filtered_out)} components without test files: "
            f"{', '.join(sorted(filtered_out))}",
            file=sys.stderr,
        )

    # Analyze all components to get their bus signatures
    component_buses, non_groupable, _direct_bus_components = analyze_all_components(
        tests_dir
    )

    # Group components by their bus signature ONLY (ignore platform)
    # All platforms will be tested by test_build_components.py for each batch
    # Key: (platform, signature), Value: list of components
    # We use ALL_PLATFORMS since batching is platform-agnostic
    signature_groups: dict[tuple[str, str], list[str]] = defaultdict(list)

    for component in components_with_tests:
        # Components that can't be grouped get unique signatures
        # This includes:
        # - Manually curated ISOLATED_COMPONENTS
        # - Automatically detected non_groupable components
        # - Directly changed components (passed via --isolate in CI)
        # These can share a batch/runner but won't be grouped/merged
        is_isolated = (
            component in ISOLATED_COMPONENTS
            or component in non_groupable
            or (directly_changed and component in directly_changed)
        )
        if is_isolated:
            signature_groups[
                (ALL_PLATFORMS, f"{ISOLATED_SIGNATURE_PREFIX}{component}")
            ].append(component)
            continue

        # Get signature from any platform (they should all have the same buses)
        # Components not in component_buses may only have variant-specific tests
        comp_platforms = component_buses.get(component)
        if not comp_platforms:
            # Component has tests but no analyzable base config - treat as no buses
            signature_groups[(ALL_PLATFORMS, NO_BUSES_SIGNATURE)].append(component)
            continue

        for platform, buses in comp_platforms.items():
            if buses:
                signature = create_grouping_signature({platform: buses}, platform)
                # Group by signature only - platform doesn't matter for batching
                # Use ALL_PLATFORMS since we're batching across all platforms
                signature_groups[(ALL_PLATFORMS, signature)].append(component)
                break  # Only use first platform for grouping
        else:
            # No buses found for any platform - can be grouped together
            signature_groups[(ALL_PLATFORMS, NO_BUSES_SIGNATURE)].append(component)

    # Merge compatible bus groups (cross-bus optimization)
    # This allows components with different buses (ble + uart) to be batched together
    # improving the efficiency of test_build_components.py grouping
    signature_groups = merge_compatible_bus_groups(signature_groups)

    # Split groups containing mutually-incompatible components (CONFLICTS_WITH).
    # Without this, batch weighting assumes the group is one build when it will
    # actually be split into two at build time -- throwing off CI distribution.
    signature_groups = split_conflicting_groups(signature_groups)

    items = [
        _make_item(
            tests_dir,
            component,
            signature,
            signature.startswith(ISOLATED_SIGNATURE_PREFIX),
        )
        for (_platform, signature), group_components in sorted(signature_groups.items())
        for component in group_components
    ]
    batches = balance_batches(items, target_seconds)

    return batches, signature_groups


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Split components into intelligent batches for CI testing"
    )
    parser.add_argument(
        "--components",
        "-c",
        required=True,
        help="JSON array of component names",
    )
    parser.add_argument(
        "--target-seconds",
        "-t",
        type=int,
        default=TARGET_BATCH_SECONDS,
        help=f"Estimated build seconds per batch (default: {TARGET_BATCH_SECONDS})",
    )
    parser.add_argument(
        "--tests-dir",
        type=Path,
        default=Path("tests/components"),
        help="Path to tests/components directory",
    )
    parser.add_argument(
        "--directly-changed",
        help="JSON array of directly changed component names (for logging only)",
    )
    parser.add_argument(
        "--output",
        "-o",
        choices=["json", "github"],
        default="github",
        help="Output format (json or github for GitHub Actions)",
    )

    args = parser.parse_args()

    # Parse component list from JSON
    try:
        components = json.loads(args.components)
    except json.JSONDecodeError as e:
        print(f"Error parsing components JSON: {e}", file=sys.stderr)
        return 1

    if not isinstance(components, list):
        print("Components must be a JSON array", file=sys.stderr)
        return 1

    # Parse directly changed components list from JSON (if provided)
    directly_changed = None
    if args.directly_changed:
        try:
            directly_changed = set(json.loads(args.directly_changed))
        except json.JSONDecodeError as e:
            print(f"Error parsing directly-changed JSON: {e}", file=sys.stderr)
            return 1

    # Create intelligent batches
    batches, signature_groups = create_intelligent_batches(
        components=components,
        tests_dir=args.tests_dir,
        target_seconds=args.target_seconds,
        directly_changed=directly_changed,
    )

    # Convert batches to space-separated strings for CI
    batch_strings = [" ".join(batch) for batch in batches]

    if args.output == "json":
        # Output as JSON array
        print(json.dumps(batch_strings))
    else:
        # Output for GitHub Actions (set output)
        output_json = json.dumps(batch_strings)
        print(f"components={output_json}")

    # Print summary to stderr so it shows in CI logs
    # Count actual components being batched
    actual_components = sum(len(batch.split()) for batch in batch_strings)

    # Re-analyze to get isolated component counts for summary
    _, non_groupable, _ = analyze_all_components(args.tests_dir)

    # Show grouping details
    print("\n=== Component Grouping Details ===", file=sys.stderr)
    # Sort groups by signature for readability
    groupable_groups = []
    isolated_groups = []
    for (_platform, signature), group_comps in sorted(signature_groups.items()):
        if signature.startswith(ISOLATED_SIGNATURE_PREFIX):
            isolated_groups.append((signature, group_comps))
        else:
            groupable_groups.append((signature, group_comps))

    if groupable_groups:
        print(
            f"\nGroupable signatures ({len(groupable_groups)} merged groups after cross-bus optimization):",
            file=sys.stderr,
        )
        for signature, group_comps in sorted(
            groupable_groups, key=lambda x: (-len(x[1]), x[0])
        ):
            # Check if this is a merged signature (contains +)
            is_merged = "+" in signature and signature != NO_BUSES_SIGNATURE
            # Special handling for no_buses components
            if signature == NO_BUSES_SIGNATURE:
                print(
                    f"  [{signature}]: {len(group_comps)} components (used as fillers across batches)",
                    file=sys.stderr,
                )
            else:
                merge_indicator = " [MERGED]" if is_merged else ""
                print(
                    f"  [{signature}]{merge_indicator}: {len(group_comps)} components",
                    file=sys.stderr,
                )
            # Show first few components as examples
            examples = ", ".join(sorted(group_comps)[:8])
            if len(group_comps) > 8:
                examples += f", ... (+{len(group_comps) - 8} more)"
            print(f"    → {examples}", file=sys.stderr)

    if isolated_groups:
        print(
            f"\nIsolated components ({len(isolated_groups)} components - tested individually):",
            file=sys.stderr,
        )
        isolated_names = sorted(
            [comp for _, comps in isolated_groups for comp in comps]
        )
        # Group isolated components for compact display
        for i in range(0, len(isolated_names), 10):
            chunk = isolated_names[i : i + 10]
            print(f"  {', '.join(chunk)}", file=sys.stderr)

    # Count isolated vs groupable components
    all_batched_components = [comp for batch in batches for comp in batch]
    isolated_count = sum(
        1
        for comp in all_batched_components
        if comp in ISOLATED_COMPONENTS
        or comp in non_groupable
        or (directly_changed and comp in directly_changed)
    )
    groupable_count = actual_components - isolated_count

    print("\n=== Intelligent Batch Summary ===", file=sys.stderr)
    print(f"Total components requested: {len(components)}", file=sys.stderr)
    print(f"Components with test files: {actual_components}", file=sys.stderr)

    # Show breakdown of directly changed vs dependencies
    if directly_changed:
        direct_count = sum(
            1 for comp in all_batched_components if comp in directly_changed
        )
        dep_count = actual_components - direct_count
        direct_comps = [
            comp for comp in all_batched_components if comp in directly_changed
        ]
        dep_comps = [
            comp for comp in all_batched_components if comp not in directly_changed
        ]
        print(
            f"  - Direct changes: {direct_count} ({', '.join(sorted(direct_comps))})",
            file=sys.stderr,
        )
        print(
            f"  - Dependencies: {dep_count} ({', '.join(sorted(dep_comps))})",
            file=sys.stderr,
        )

    print(f"  - Groupable: {groupable_count}", file=sys.stderr)
    print(f"  - Isolated: {isolated_count}", file=sys.stderr)
    if actual_components < len(components):
        print(
            f"Components skipped (no test files): {len(components) - actual_components}",
            file=sys.stderr,
        )
    print(f"Number of batches: {len(batches)}", file=sys.stderr)
    print(f"Target build seconds per batch: {args.target_seconds}", file=sys.stderr)
    if len(batches) > 0:
        print(
            f"Average components per batch: {actual_components / len(batches):.1f}",
            file=sys.stderr,
        )
    print(file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
