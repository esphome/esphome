#!/usr/bin/env python3
"""Analyze component test files to detect which common bus configs they use.

This script scans component test files and extracts which common bus configurations
(i2c, spi, uart, etc.) are included via the packages mechanism. This information
is used to group components that can be tested together.

Components can only be grouped together if they use the EXACT SAME set of common
bus configurations, ensuring that merged configs are compatible.

Example output:
{
    "component1": {
        "esp32-ard": ["i2c", "uart_19200"],
        "esp32-idf": ["i2c", "uart_19200"]
    },
    "component2": {
        "esp32-ard": ["spi"],
        "esp32-idf": ["spi"]
    }
}
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

# Path to common bus configs
COMMON_BUS_PATH = Path("tests/test_build_components/common")

# Valid common bus config directories
VALID_BUS_CONFIGS = {
    "ble",
    "i2c",
    "i2c_low_freq",
    "qspi",
    "spi",
    "uart",
    "uart_115200",
    "uart_1200",
    "uart_1200_even",
    "uart_19200",
    "uart_38400",
    "uart_4800",
    "uart_4800_even",
    "uart_9600_even",
}


def extract_common_buses(yaml_file: Path) -> set[str]:
    """Extract which common bus configs are included in a YAML test file.

    Args:
        yaml_file: Path to the component test YAML file

    Returns:
        Set of common bus config names (e.g., {'i2c', 'uart_19200'})
    """
    if not yaml_file.exists():
        return set()

    try:
        content = yaml_file.read_text()
    except Exception:
        return set()

    buses = set()

    # Pattern to match package includes for common bus configs
    # Matches: !include ../../test_build_components/common/{bus}/{platform}.yaml
    pattern = r"!include\s+\.\./\.\./test_build_components/common/([^/]+)/"

    for match in re.finditer(pattern, content):
        bus_name = match.group(1)
        if bus_name in VALID_BUS_CONFIGS:
            buses.add(bus_name)

    return buses


def analyze_component(component_dir: Path) -> dict[str, list[str]]:
    """Analyze a component directory to find which buses each platform uses.

    Args:
        component_dir: Path to the component's test directory

    Returns:
        Dictionary mapping platform to list of bus configs
        Example: {"esp32-ard": ["i2c", "spi"], "esp32-idf": ["i2c"]}
    """
    if not component_dir.is_dir():
        return {}

    platform_buses = {}

    # Find all test.*.yaml files
    for test_file in component_dir.glob("test.*.yaml"):
        # Extract platform name from filename (e.g., test.esp32-ard.yaml -> esp32-ard)
        platform = test_file.stem.replace("test.", "")

        buses = extract_common_buses(test_file)
        if buses:
            # Sort for consistent comparison
            platform_buses[platform] = sorted(buses)

    return platform_buses


def analyze_all_components(tests_dir: Path = None) -> dict[str, dict[str, list[str]]]:
    """Analyze all component test directories.

    Args:
        tests_dir: Path to tests/components directory (defaults to auto-detect)

    Returns:
        Dictionary mapping component name to platform->buses mapping
    """
    if tests_dir is None:
        tests_dir = Path("tests/components")

    if not tests_dir.exists():
        print(f"Error: {tests_dir} does not exist", file=sys.stderr)
        return {}

    components = {}

    for component_dir in sorted(tests_dir.iterdir()):
        if not component_dir.is_dir():
            continue

        component_name = component_dir.name
        platform_buses = analyze_component(component_dir)

        if platform_buses:
            components[component_name] = platform_buses

    return components


def create_grouping_signature(
    platform_buses: dict[str, list[str]], platform: str
) -> str:
    """Create a signature string for grouping components.

    Components with the same signature can be grouped together for testing.

    Args:
        platform_buses: Mapping of platform to list of buses
        platform: The specific platform to create signature for

    Returns:
        Signature string (e.g., "i2c+uart_19200" or "spi")
    """
    buses = platform_buses.get(platform, [])
    if not buses:
        return ""
    return "+".join(sorted(buses))


def group_components_by_signature(
    components: dict[str, dict[str, list[str]]], platform: str
) -> dict[str, list[str]]:
    """Group components by their bus signature for a specific platform.

    Args:
        components: Component analysis results from analyze_all_components()
        platform: Platform to group for (e.g., "esp32-ard")

    Returns:
        Dictionary mapping signature to list of component names
        Example: {"i2c+uart_19200": ["comp1", "comp2"], "spi": ["comp3"]}
    """
    signature_groups: dict[str, list[str]] = {}

    for component_name, platform_buses in components.items():
        if platform not in platform_buses:
            continue

        signature = create_grouping_signature(platform_buses, platform)
        if not signature:
            continue

        if signature not in signature_groups:
            signature_groups[signature] = []
        signature_groups[signature].append(component_name)

    return signature_groups


def main() -> None:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Analyze component test files to detect common bus usage"
    )
    parser.add_argument(
        "--components",
        "-c",
        nargs="+",
        help="Specific components to analyze (default: all)",
    )
    parser.add_argument(
        "--platform",
        "-p",
        help="Show grouping for a specific platform",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output as JSON",
    )
    parser.add_argument(
        "--group",
        action="store_true",
        help="Show component groupings by bus signature",
    )

    args = parser.parse_args()

    # Analyze components
    tests_dir = Path("tests/components")

    if args.components:
        # Analyze only specified components
        components = {}
        for comp in args.components:
            comp_dir = tests_dir / comp
            platform_buses = analyze_component(comp_dir)
            if platform_buses:
                components[comp] = platform_buses
    else:
        # Analyze all components
        components = analyze_all_components(tests_dir)

    # Output results
    if args.group and args.platform:
        # Show groupings for a specific platform
        groups = group_components_by_signature(components, args.platform)

        if args.json:
            print(json.dumps(groups, indent=2))
        else:
            print(f"Component groupings for {args.platform}:")
            print()
            for signature, comp_list in sorted(groups.items()):
                print(f"  {signature}:")
                for comp in sorted(comp_list):
                    print(f"    - {comp}")
                print()
    elif args.json:
        # JSON output
        print(json.dumps(components, indent=2))
    else:
        # Human-readable output
        for component, platform_buses in sorted(components.items()):
            print(f"{component}:")
            for platform, buses in sorted(platform_buses.items()):
                bus_str = ", ".join(buses)
                print(f"  {platform}: {bus_str}")
        print()
        print(f"Total components analyzed: {len(components)}")


if __name__ == "__main__":
    main()
