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
import json
from pathlib import Path
import sys

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from script.analyze_component_buses import (
    ISOLATED_COMPONENTS,
    NO_BUSES_SIGNATURE,
    analyze_all_components,
    create_grouping_signature,
)

# Weighting for batch creation
# Isolated components can't be grouped/merged, so they count as 10x
# Groupable components can be merged into single builds, so they count as 1x
ISOLATED_WEIGHT = 10
GROUPABLE_WEIGHT = 1


def has_test_files(component_name: str, tests_dir: Path) -> bool:
    """Check if a component has test files.

    Args:
        component_name: Name of the component
        tests_dir: Path to tests/components directory

    Returns:
        True if the component has test.*.yaml files
    """
    component_dir = tests_dir / component_name
    if not component_dir.exists() or not component_dir.is_dir():
        return False

    # Check for test.*.yaml files
    return any(component_dir.glob("test.*.yaml"))


def create_intelligent_batches(
    components: list[str],
    tests_dir: Path,
    batch_size: int = 40,
) -> list[list[str]]:
    """Create batches optimized for component grouping.

    Args:
        components: List of component names to batch
        tests_dir: Path to tests/components directory
        batch_size: Target size for each batch

    Returns:
        List of component batches (lists of component names)
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
    # Key: signature, Value: list of components
    signature_groups: dict[str, list[str]] = defaultdict(list)

    for component in components_with_tests:
        # Components that can't be grouped get unique signatures
        # This includes both manually curated ISOLATED_COMPONENTS and
        # automatically detected non_groupable components
        # These can share a batch/runner but won't be grouped/merged
        if component in ISOLATED_COMPONENTS or component in non_groupable:
            signature_groups[f"isolated_{component}"].append(component)
            continue

        # Get signature from any platform (they should all have the same buses)
        # Components not in component_buses were filtered out by has_test_files check
        comp_platforms = component_buses[component]
        for platform, buses in comp_platforms.items():
            if buses:
                signature = create_grouping_signature({platform: buses}, platform)
                # Group by signature only - platform doesn't matter for batching
                signature_groups[signature].append(component)
                break  # Only use first platform for grouping
        else:
            # No buses found for any platform - can be grouped together
            signature_groups[NO_BUSES_SIGNATURE].append(component)

    # Create batches by keeping signature groups together
    # Components with the same signature stay in the same batches
    batches = []

    # Sort signature groups to prioritize groupable components
    # 1. Put "isolated_*" signatures last (can't be grouped with others)
    # 2. Sort groupable signatures by size (largest first)
    # 3. "no_buses" components CAN be grouped together
    def sort_key(item):
        signature, components = item
        is_isolated = signature.startswith("isolated_")
        # Put "isolated_*" last (1), groupable first (0)
        # Within each category, sort by size (largest first)
        return (is_isolated, -len(components))

    sorted_groups = sorted(signature_groups.items(), key=sort_key)

    # Strategy: Create batches using weighted sizes
    # - Isolated components count as 10x (since they can't be grouped/merged)
    # - Groupable components count as 1x (can be merged into single builds)
    # - This distributes isolated components across more runners
    # - Ensures each runner has a good mix of groupable vs isolated components

    current_batch = []
    current_weight = 0

    for signature, group_components in sorted_groups:
        is_isolated = signature.startswith("isolated_")
        weight_per_component = ISOLATED_WEIGHT if is_isolated else GROUPABLE_WEIGHT

        for component in group_components:
            # Check if adding this component would exceed the batch size
            if current_weight + weight_per_component > batch_size and current_batch:
                # Start a new batch
                batches.append(current_batch)
                current_batch = []
                current_weight = 0

            # Add component to current batch
            current_batch.append(component)
            current_weight += weight_per_component

    # Don't forget the last batch
    if current_batch:
        batches.append(current_batch)

    return batches


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
        "--batch-size",
        "-b",
        type=int,
        default=40,
        help="Target batch size (default: 40, weighted)",
    )
    parser.add_argument(
        "--tests-dir",
        type=Path,
        default=Path("tests/components"),
        help="Path to tests/components directory",
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

    # Create intelligent batches
    batches = create_intelligent_batches(
        components=components,
        tests_dir=args.tests_dir,
        batch_size=args.batch_size,
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

    # Count isolated vs groupable components
    all_batched_components = [comp for batch in batches for comp in batch]
    isolated_count = sum(
        1
        for comp in all_batched_components
        if comp in ISOLATED_COMPONENTS or comp in non_groupable
    )
    groupable_count = actual_components - isolated_count

    print("\n=== Intelligent Batch Summary ===", file=sys.stderr)
    print(f"Total components requested: {len(components)}", file=sys.stderr)
    print(f"Components with test files: {actual_components}", file=sys.stderr)
    print(f"  - Groupable (weight=1): {groupable_count}", file=sys.stderr)
    print(f"  - Isolated (weight=10): {isolated_count}", file=sys.stderr)
    if actual_components < len(components):
        print(
            f"Components skipped (no test files): {len(components) - actual_components}",
            file=sys.stderr,
        )
    print(f"Number of batches: {len(batches)}", file=sys.stderr)
    print(f"Batch size target (weighted): {args.batch_size}", file=sys.stderr)
    if len(batches) > 0:
        print(
            f"Average components per batch: {actual_components / len(batches):.1f}",
            file=sys.stderr,
        )
    print(file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
