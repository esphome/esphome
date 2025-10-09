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
    analyze_all_components,
    create_grouping_signature,
)


def create_intelligent_batches(
    components: list[str],
    tests_dir: Path,
    batch_size: int = 10,
) -> list[list[str]]:
    """Create batches optimized for component grouping.

    Args:
        components: List of component names to batch
        tests_dir: Path to tests/components directory
        batch_size: Target size for each batch

    Returns:
        List of component batches (lists of component names)
    """
    # Analyze all components to get their bus signatures
    component_buses = analyze_all_components(tests_dir)

    # Group components by their bus signature
    # Key: (platform, signature), Value: list of components
    signature_groups: dict[tuple[str, str], list[str]] = defaultdict(list)

    for component in components:
        if component not in component_buses:
            # Component has no bus configs, put in special group
            signature_groups[("none", "none")].append(component)
            continue

        # Group by platform and signature
        comp_platforms = component_buses[component]
        for platform, buses in comp_platforms.items():
            if buses:
                signature = create_grouping_signature({platform: buses}, platform)
                signature_groups[(platform, signature)].append(component)
                break  # Only use first platform for grouping
        else:
            # No buses found
            signature_groups[("none", "none")].append(component)

    # Create batches by keeping signature groups together
    # Components with the same signature stay in the same batches
    batches = []

    # Sort signature groups to prioritize groupable components
    # 1. Put "none" signature last (can't be grouped)
    # 2. Sort groupable signatures by size (largest first)
    def sort_key(item):
        (platform, signature), components = item
        is_none = platform == "none" and signature == "none"
        # Put "none" last (1), groupable first (0)
        # Within each category, sort by size (largest first)
        return (is_none, -len(components))

    sorted_groups = sorted(signature_groups.items(), key=sort_key)

    for (platform, signature), group_components in sorted_groups:
        # Split this signature group into batches of batch_size
        # This keeps components with the same signature together
        sorted_components = sorted(group_components)  # Sort for determinism

        for i in range(0, len(sorted_components), batch_size):
            batch = sorted_components[i : i + batch_size]
            batches.append(batch)

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
        default=10,
        help="Target batch size (default: 10)",
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
    print("\n=== Intelligent Batch Summary ===", file=sys.stderr)
    print(f"Total components: {len(components)}", file=sys.stderr)
    print(f"Number of batches: {len(batches)}", file=sys.stderr)
    print(f"Batch size target: {args.batch_size}", file=sys.stderr)
    print(f"Average batch size: {len(components) / len(batches):.1f}", file=sys.stderr)
    print(file=sys.stderr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
