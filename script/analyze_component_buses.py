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

# Bus types that support component grouping for config validation
# I2C and SPI are shared buses with addressing/CS pins
# BLE sensors scan independently and don't conflict
# UART is point-to-point but can be grouped for testing since --testing-mode
# bypasses the conflict validation (we only care that configs compile)
GROUPABLE_BUS_TYPES = {
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


def uses_local_file_references(component_dir: Path) -> bool:
    """Check if a component uses local file references via $component_dir.

    Components that reference local files cannot be grouped because each needs
    a unique component_dir path pointing to their specific directory.

    Args:
        component_dir: Path to the component's test directory

    Returns:
        True if the component uses $component_dir for local file references
    """
    common_yaml = component_dir / "common.yaml"
    if not common_yaml.exists():
        return False

    try:
        content = common_yaml.read_text()
    except Exception:
        return False

    # Pattern to match $component_dir or ${component_dir} references
    # These indicate local file usage that prevents grouping
    return bool(re.search(r"\$\{?component_dir\}?", content))


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


def analyze_all_components(
    tests_dir: Path = None,
) -> tuple[dict[str, dict[str, list[str]]], set[str]]:
    """Analyze all component test directories.

    Args:
        tests_dir: Path to tests/components directory (defaults to auto-detect)

    Returns:
        Tuple of:
        - Dictionary mapping component name to platform->buses mapping
        - Set of component names that use local files (cannot be grouped)
    """
    if tests_dir is None:
        tests_dir = Path("tests/components")

    if not tests_dir.exists():
        print(f"Error: {tests_dir} does not exist", file=sys.stderr)
        return {}, set()

    components = {}
    non_groupable = set()

    for component_dir in sorted(tests_dir.iterdir()):
        if not component_dir.is_dir():
            continue

        component_name = component_dir.name
        platform_buses = analyze_component(component_dir)

        if platform_buses:
            components[component_name] = platform_buses

        # Check if component uses local file references
        if uses_local_file_references(component_dir):
            non_groupable.add(component_name)

    return components, non_groupable


def create_grouping_signature(
    platform_buses: dict[str, list[str]], platform: str
) -> str:
    """Create a signature string for grouping components.

    Components with the same signature can be grouped together for testing.
    Includes groupable bus types (I2C, SPI, BLE, UART) that can be validated
    together using --testing-mode to bypass runtime conflicts.

    Args:
        platform_buses: Mapping of platform to list of buses
        platform: The specific platform to create signature for

    Returns:
        Signature string (e.g., "i2c" or "uart") or empty if no groupable buses
    """
    buses = platform_buses.get(platform, [])
    if not buses:
        return ""

    # Only include groupable bus types in signature
    groupable_buses = [b for b in buses if b in GROUPABLE_BUS_TYPES]
    if not groupable_buses:
        return ""

    return "+".join(sorted(groupable_buses))


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
        non_groupable = set()
        for comp in args.components:
            comp_dir = tests_dir / comp
            platform_buses = analyze_component(comp_dir)
            if platform_buses:
                components[comp] = platform_buses
            if uses_local_file_references(comp_dir):
                non_groupable.add(comp)
    else:
        # Analyze all components
        components, non_groupable = analyze_all_components(tests_dir)

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
            non_groupable_marker = (
                " [NON-GROUPABLE]" if component in non_groupable else ""
            )
            print(f"{component}{non_groupable_marker}:")
            for platform, buses in sorted(platform_buses.items()):
                bus_str = ", ".join(buses)
                print(f"  {platform}: {bus_str}")
        print()
        print(f"Total components analyzed: {len(components)}")
        if non_groupable:
            print(f"Non-groupable components (use local files): {len(non_groupable)}")
            for comp in sorted(non_groupable):
                print(f"  - {comp}")


if __name__ == "__main__":
    main()
