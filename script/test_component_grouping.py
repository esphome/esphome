#!/usr/bin/env python3
"""Test component grouping by finding and testing groups of components.

This script analyzes components, finds groups that can be tested together,
and runs test builds for those groups.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from script.analyze_component_buses import (
    analyze_all_components,
    group_components_by_signature,
)


def test_component_group(
    components: list[str],
    platform: str,
    esphome_command: str = "compile",
    dry_run: bool = False,
) -> bool:
    """Test a group of components together.

    Args:
        components: List of component names to test together
        platform: Platform to test on (e.g., "esp32-idf")
        esphome_command: ESPHome command to run (config/compile/clean)
        dry_run: If True, only print the command without running it

    Returns:
        True if test passed, False otherwise
    """
    components_str = ",".join(components)
    cmd = [
        "./script/test_build_components",
        "-c",
        components_str,
        "-t",
        platform,
        "-e",
        esphome_command,
    ]

    print(f"\n{'=' * 80}")
    print(f"Testing {len(components)} components on {platform}:")
    for comp in components:
        print(f"  - {comp}")
    print(f"{'=' * 80}")
    print(f"Command: {' '.join(cmd)}\n")

    if dry_run:
        print("[DRY RUN] Skipping actual test")
        return True

    try:
        result = subprocess.run(cmd, check=False)
        return result.returncode == 0
    except Exception as e:
        print(f"Error running test: {e}")
        return False


def main() -> None:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Test component grouping by finding and testing groups"
    )
    parser.add_argument(
        "--platform",
        "-p",
        default="esp32-idf",
        help="Platform to test (default: esp32-idf)",
    )
    parser.add_argument(
        "-e",
        "--esphome-command",
        default="compile",
        choices=["config", "compile", "clean"],
        help="ESPHome command to run (default: compile)",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Test all components (sets --min-size=1, --max-size=10000, --max-groups=10000)",
    )
    parser.add_argument(
        "--min-size",
        type=int,
        default=3,
        help="Minimum group size to test (default: 3)",
    )
    parser.add_argument(
        "--max-size",
        type=int,
        default=10,
        help="Maximum group size to test (default: 10)",
    )
    parser.add_argument(
        "--max-groups",
        type=int,
        default=5,
        help="Maximum number of groups to test (default: 5)",
    )
    parser.add_argument(
        "--signature",
        "-s",
        help="Only test groups with this bus signature (e.g., 'spi', 'i2c', 'uart')",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without running them",
    )

    args = parser.parse_args()

    # If --all is specified, test all components without grouping
    if args.all:
        # Get all components from tests/components directory
        components_dir = Path("tests/components")
        all_components = sorted(
            [d.name for d in components_dir.iterdir() if d.is_dir()]
        )

        if not all_components:
            print(f"\nNo components found in {components_dir}")
            return

        print(f"\nTesting all {len(all_components)} components together")

        success = test_component_group(
            all_components, args.platform, args.esphome_command, args.dry_run
        )

        # Print summary
        print(f"\n{'=' * 80}")
        print("TEST SUMMARY")
        print(f"{'=' * 80}")
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{status} All components: {len(all_components)} components")

        if not args.dry_run and not success:
            sys.exit(1)
        return

    print("Analyzing all components...")
    components, non_groupable, _ = analyze_all_components(Path("tests/components"))

    print(f"Found {len(components)} components, {len(non_groupable)} non-groupable")

    # Group components by signature for the platform
    groups = group_components_by_signature(components, args.platform)

    # Filter and sort groups
    filtered_groups = []
    for signature, comp_list in groups.items():
        # Filter by signature if specified
        if args.signature and signature != args.signature:
            continue

        # Remove non-groupable components
        comp_list = [c for c in comp_list if c not in non_groupable]

        # Filter by minimum size
        if len(comp_list) < args.min_size:
            continue

        # If group is larger than max_size, we'll take a subset later
        filtered_groups.append((signature, comp_list))

    # Sort by group size (largest first)
    filtered_groups.sort(key=lambda x: len(x[1]), reverse=True)

    # Limit number of groups
    filtered_groups = filtered_groups[: args.max_groups]

    if not filtered_groups:
        print("\nNo groups found matching criteria:")
        print(f"  - Platform: {args.platform}")
        print(f"  - Size: {args.min_size}-{args.max_size}")
        if args.signature:
            print(f"  - Signature: {args.signature}")
        return

    print(f"\nFound {len(filtered_groups)} groups to test:")
    for signature, comp_list in filtered_groups:
        print(f"  [{signature}]: {len(comp_list)} components")

    # Test each group
    results = []
    for signature, comp_list in filtered_groups:
        # Limit to max_size if group is larger
        if len(comp_list) > args.max_size:
            comp_list = comp_list[: args.max_size]

        success = test_component_group(
            comp_list, args.platform, args.esphome_command, args.dry_run
        )
        results.append((signature, comp_list, success))

        if not args.dry_run and not success:
            print(f"\n❌ FAILED: {signature} group")
            break

    # Print summary
    print(f"\n{'=' * 80}")
    print("TEST SUMMARY")
    print(f"{'=' * 80}")
    for signature, comp_list, success in results:
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{status} [{signature}]: {len(comp_list)} components")

    # Exit with error if any tests failed
    if not args.dry_run and any(not success for _, _, success in results):
        sys.exit(1)


if __name__ == "__main__":
    main()
