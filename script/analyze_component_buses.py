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
from functools import lru_cache
import json
from pathlib import Path
import re
import sys
from typing import Any

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from esphome import yaml_util
from esphome.config_helpers import Extend, Remove

# Path to common bus configs
COMMON_BUS_PATH = Path("tests/test_build_components/common")

# Package dependencies - maps packages to the packages they include
# When a component uses a package on the left, it automatically gets
# the packages on the right as well
PACKAGE_DEPENDENCIES = {
    "modbus": ["uart"],  # modbus packages include uart packages
    # Add more package dependencies here as needed
}

# Bus types that can be defined directly in config files
# Components defining these directly cannot be grouped (they create unique bus IDs)
DIRECT_BUS_TYPES = ("i2c", "spi", "uart", "modbus")

# Signature for components with no bus requirements
# These components can be merged with any other group
NO_BUSES_SIGNATURE = "no_buses"

# Base bus components - these ARE the bus implementations and should not
# be flagged as needing migration since they are the platform/base components
BASE_BUS_COMPONENTS = {
    "i2c",
    "spi",
    "uart",
    "modbus",
    "canbus",
}

# Components that must be tested in isolation (not grouped or batched with others)
# These have known build issues that prevent grouping
# NOTE: This should be kept in sync with both test_build_components and split_components_for_ci.py
ISOLATED_COMPONENTS = {
    "animation": "Has display lambda in common.yaml that requires existing display platform - breaks when merged without display",
    "esphome": "Defines devices/areas in esphome: section that are referenced in other sections - breaks when merged",
    "ethernet": "Defines ethernet: which conflicts with wifi: used by most components",
    "ethernet_info": "Related to ethernet component which conflicts with wifi",
    "lvgl": "Defines multiple SDL displays on host platform that conflict when merged with other display configs",
    "openthread": "Conflicts with wifi: used by most components",
    "openthread_info": "Conflicts with wifi: used by most components",
    "matrix_keypad": "Needs isolation due to keypad",
    "mcp4725": "no YAML config to specify i2c bus id",
    "mcp47a1": "no YAML config to specify i2c bus id",
    "modbus_controller": "Defines multiple modbus buses for testing client/server functionality - conflicts with package modbus bus",
    "neopixelbus": "RMT type conflict with ESP32 Arduino/ESP-IDF headers (enum vs struct rmt_channel_t)",
    "packages": "cannot merge packages",
}


@lru_cache(maxsize=1)
def get_common_bus_packages() -> frozenset[str]:
    """Get the list of common bus package names.

    Reads from tests/test_build_components/common/ directory
    and caches the result. All bus types support component grouping
    for config validation since --testing-mode bypasses runtime conflicts.

    Returns:
        Frozenset of common bus package names (i2c, spi, uart, etc.)
    """
    if not COMMON_BUS_PATH.exists():
        return frozenset()

    # List all directories in common/ - these are the bus package names
    return frozenset(d.name for d in COMMON_BUS_PATH.iterdir() if d.is_dir())


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
    except Exception:  # pylint: disable=broad-exception-caught
        return False

    # Pattern to match $component_dir or ${component_dir} references
    # These indicate local file usage that prevents grouping
    return bool(re.search(r"\$\{?component_dir\}?", content))


def is_platform_component(component_dir: Path) -> bool:
    """Check if a component is a platform component (abstract base class).

    Platform components have IS_PLATFORM_COMPONENT = True and cannot be
    instantiated without a platform-specific implementation. These components
    define abstract methods and cause linker errors if compiled standalone.

    Examples: canbus, mcp23x08_base, mcp23x17_base

    Args:
        component_dir: Path to the component's test directory

    Returns:
        True if this is a platform component
    """
    # Check in the actual component source, not tests
    # tests/components/X -> tests/components -> tests -> repo root
    repo_root = component_dir.parent.parent.parent
    comp_init = (
        repo_root / "esphome" / "components" / component_dir.name / "__init__.py"
    )

    if not comp_init.exists():
        return False

    try:
        content = comp_init.read_text()
        return "IS_PLATFORM_COMPONENT = True" in content
    except Exception:  # pylint: disable=broad-exception-caught
        return False


def _contains_extend_or_remove(data: Any) -> bool:
    """Recursively check if data contains Extend or Remove objects.

    Args:
        data: Parsed YAML data structure

    Returns:
        True if any Extend or Remove objects are found
    """
    if isinstance(data, (Extend, Remove)):
        return True

    if isinstance(data, dict):
        for value in data.values():
            if _contains_extend_or_remove(value):
                return True

    if isinstance(data, list):
        for item in data:
            if _contains_extend_or_remove(item):
                return True

    return False


def analyze_yaml_file(yaml_file: Path) -> dict[str, Any]:
    """Load a YAML file once and extract all needed information.

    This loads the YAML file a single time and extracts all information needed
    for component analysis, avoiding multiple file reads.

    Args:
        yaml_file: Path to the YAML file to analyze

    Returns:
        Dictionary with keys:
        - buses: set of common bus package names
        - has_extend_remove: bool indicating if Extend/Remove objects are present
        - has_direct_bus_config: bool indicating if buses are defined directly (not via packages)
        - loaded: bool indicating if file was successfully loaded
    """
    result = {
        "buses": set(),
        "has_extend_remove": False,
        "has_direct_bus_config": False,
        "loaded": False,
    }

    if not yaml_file.exists():
        return result

    try:
        data = yaml_util.load_yaml(yaml_file)
        result["loaded"] = True
    except Exception:  # pylint: disable=broad-exception-caught
        return result

    # Check for Extend/Remove objects
    result["has_extend_remove"] = _contains_extend_or_remove(data)

    # Check if buses are defined directly (not via packages)
    # Components that define i2c, spi, uart, or modbus directly in test files
    # cannot be grouped because they create unique bus IDs
    if isinstance(data, dict):
        for bus_type in DIRECT_BUS_TYPES:
            if bus_type in data:
                result["has_direct_bus_config"] = True
                break

    # Extract common bus packages
    if not isinstance(data, dict) or "packages" not in data:
        return result

    packages = data["packages"]
    if not isinstance(packages, dict):
        return result

    valid_buses = get_common_bus_packages()
    for pkg_name in packages:
        if pkg_name not in valid_buses:
            continue
        result["buses"].add(pkg_name)
        # Add any package dependencies (e.g., modbus includes uart)
        if pkg_name not in PACKAGE_DEPENDENCIES:
            continue
        for dep in PACKAGE_DEPENDENCIES[pkg_name]:
            if dep not in valid_buses:
                continue
            result["buses"].add(dep)

    return result


def analyze_component(component_dir: Path) -> tuple[dict[str, list[str]], bool, bool]:
    """Analyze a component directory to find which buses each platform uses.

    Args:
        component_dir: Path to the component's test directory

    Returns:
        Tuple of:
        - Dictionary mapping platform to list of bus configs
          Example: {"esp32-ard": ["i2c", "spi"], "esp32-idf": ["i2c"]}
        - Boolean indicating if component uses !extend or !remove
        - Boolean indicating if component defines buses directly (not via packages)
    """
    if not component_dir.is_dir():
        return {}, False, False

    platform_buses = {}
    has_extend_remove = False
    has_direct_bus_config = False

    # Analyze all YAML files in the component directory
    for yaml_file in component_dir.glob("*.yaml"):
        analysis = analyze_yaml_file(yaml_file)

        # Track if any file uses extend/remove
        if analysis["has_extend_remove"]:
            has_extend_remove = True

        # Track if any file defines buses directly
        if analysis["has_direct_bus_config"]:
            has_direct_bus_config = True

        # For test.*.yaml files, extract platform and buses
        if yaml_file.name.startswith("test.") and yaml_file.suffix == ".yaml":
            # Extract platform name (e.g., test.esp32-ard.yaml -> esp32-ard)
            platform = yaml_file.stem.replace("test.", "")
            # Always add platform, even if it has no buses (empty list)
            # This allows grouping components that don't use any shared buses
            platform_buses[platform] = (
                sorted(analysis["buses"]) if analysis["buses"] else []
            )

    return platform_buses, has_extend_remove, has_direct_bus_config


def analyze_all_components(
    tests_dir: Path = None,
) -> tuple[dict[str, dict[str, list[str]]], set[str], set[str]]:
    """Analyze all component test directories.

    Args:
        tests_dir: Path to tests/components directory (defaults to auto-detect)

    Returns:
        Tuple of:
        - Dictionary mapping component name to platform->buses mapping
        - Set of component names that cannot be grouped
        - Set of component names that define buses directly (need migration warning)
    """
    if tests_dir is None:
        tests_dir = Path("tests/components")

    if not tests_dir.exists():
        print(f"Error: {tests_dir} does not exist", file=sys.stderr)
        return {}, set(), set()

    components = {}
    non_groupable = set()
    direct_bus_components = set()

    for component_dir in sorted(tests_dir.iterdir()):
        if not component_dir.is_dir():
            continue

        component_name = component_dir.name
        platform_buses, has_extend_remove, has_direct_bus_config = analyze_component(
            component_dir
        )

        if platform_buses:
            components[component_name] = platform_buses

        # Note: Components using $component_dir are now groupable because the merge
        # script rewrites these to absolute paths with component-specific substitutions

        # Check if component is explicitly isolated
        # These have known issues that prevent grouping with other components
        if component_name in ISOLATED_COMPONENTS:
            non_groupable.add(component_name)

        # Check if component is a base bus component
        # These ARE the bus platform implementations and define buses directly for testing
        # They cannot be grouped with components that use bus packages (causes ID conflicts)
        if component_name in BASE_BUS_COMPONENTS:
            non_groupable.add(component_name)

        # Check if component uses !extend or !remove directives
        # These rely on specific config structure and cannot be merged with other components
        # The directives work within a component's own package hierarchy but break when
        # merging independent components together
        if has_extend_remove:
            non_groupable.add(component_name)

        # Check if component defines buses directly in test files
        # These create unique bus IDs and cause conflicts when merged
        # Exclude base bus components (i2c, spi, uart, etc.) since they ARE the platform
        if has_direct_bus_config and component_name not in BASE_BUS_COMPONENTS:
            non_groupable.add(component_name)
            direct_bus_components.add(component_name)

    return components, non_groupable, direct_bus_components


def create_grouping_signature(
    platform_buses: dict[str, list[str]], platform: str
) -> str:
    """Create a signature string for grouping components.

    Components with the same signature can be grouped together for testing.
    All valid bus types can be grouped since --testing-mode bypasses runtime
    conflicts during config validation.

    Args:
        platform_buses: Mapping of platform to list of buses
        platform: The specific platform to create signature for

    Returns:
        Signature string (e.g., "i2c" or "uart") or empty if no valid buses
    """
    buses = platform_buses.get(platform, [])
    if not buses:
        return ""

    # Only include valid bus types in signature
    common_buses = get_common_bus_packages()
    valid_buses = [b for b in buses if b in common_buses]
    if not valid_buses:
        return ""

    return "+".join(sorted(valid_buses))


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
        direct_bus_components = set()
        for comp in args.components:
            comp_dir = tests_dir / comp
            platform_buses, has_extend_remove, has_direct_bus_config = (
                analyze_component(comp_dir)
            )
            if platform_buses:
                components[comp] = platform_buses
            # Note: Components using $component_dir are now groupable
            if comp in ISOLATED_COMPONENTS:
                non_groupable.add(comp)
            if comp in BASE_BUS_COMPONENTS:
                non_groupable.add(comp)
            if has_direct_bus_config and comp not in BASE_BUS_COMPONENTS:
                non_groupable.add(comp)
                direct_bus_components.add(comp)
    else:
        # Analyze all components
        components, non_groupable, direct_bus_components = analyze_all_components(
            tests_dir
        )

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
