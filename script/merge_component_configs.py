#!/usr/bin/env python3
"""Merge multiple component test configurations into a single test file.

This script combines multiple component test files that use the same common bus
configurations into a single merged test file. This allows testing multiple
compatible components together, reducing CI build time.

The merger handles:
- Component-specific substitutions (prefixing to avoid conflicts)
- Multiple instances of component configurations
- Shared common bus packages (included only once)
- Platform-specific configurations
- Uses ESPHome's built-in merge_config for proper YAML merging
"""

from __future__ import annotations

import argparse
from functools import lru_cache
from pathlib import Path
import re
import sys
from typing import Any

# Add esphome to path so we can import from it
sys.path.insert(0, str(Path(__file__).parent.parent))

from esphome import yaml_util
from esphome.config_helpers import merge_config
from script.analyze_component_buses import PACKAGE_DEPENDENCIES, get_common_bus_packages

# Prefix for dependency markers in package tracking
# Used to mark packages that are included transitively (e.g., uart via modbus)
DEPENDENCY_MARKER_PREFIX = "_dep_"


def load_yaml_file(yaml_file: Path) -> dict:
    """Load YAML file using ESPHome's YAML loader.

    Args:
        yaml_file: Path to the YAML file

    Returns:
        Parsed YAML as dictionary
    """
    if not yaml_file.exists():
        raise FileNotFoundError(f"YAML file not found: {yaml_file}")

    return yaml_util.load_yaml(yaml_file)


@lru_cache(maxsize=256)
def get_component_packages(
    component_name: str, platform: str, tests_dir_str: str
) -> dict:
    """Get packages dict from a component's test file with caching.

    This function is cached to avoid re-loading and re-parsing the same file
    multiple times when extracting packages during cross-bus merging.

    Args:
        component_name: Name of the component
        platform: Platform name (e.g., "esp32-idf")
        tests_dir_str: String path to tests/components directory (must be string for cache hashability)

    Returns:
        Dictionary with 'packages' key containing the raw packages dict from the YAML,
        or empty dict if no packages section exists
    """
    tests_dir = Path(tests_dir_str)
    test_file = tests_dir / component_name / f"test.{platform}.yaml"
    comp_data = load_yaml_file(test_file)

    if "packages" not in comp_data or not isinstance(comp_data["packages"], dict):
        return {}

    return comp_data["packages"]


def extract_packages_from_yaml(data: dict) -> dict[str, str]:
    """Extract COMMON BUS package includes from parsed YAML.

    Only extracts packages that are from test_build_components/common/,
    ignoring component-specific packages.

    Args:
        data: Parsed YAML dictionary

    Returns:
        Dictionary mapping package name to include path (as string representation)
        Only includes common bus packages (i2c, spi, uart, etc.)
    """
    if "packages" not in data:
        return {}

    packages_value = data["packages"]
    if not isinstance(packages_value, dict):
        # List format doesn't include common bus packages (those use dict format)
        return {}

    # Get common bus package names (cached)
    common_bus_packages = get_common_bus_packages()
    packages = {}

    # Dictionary format: packages: {name: value}
    for name, value in packages_value.items():
        # Only include common bus packages, ignore component-specific ones
        if name not in common_bus_packages:
            continue
        packages[name] = str(value)
        # Also track package dependencies (e.g., modbus includes uart)
        if name not in PACKAGE_DEPENDENCIES:
            continue
        for dep in PACKAGE_DEPENDENCIES[name]:
            if dep not in common_bus_packages:
                continue
            # Mark as included via dependency
            packages[f"{DEPENDENCY_MARKER_PREFIX}{dep}"] = f"(included via {name})"

    return packages


def prefix_substitutions_in_dict(
    data: Any, prefix: str, exclude: set[str] | None = None
) -> Any:
    """Recursively prefix all substitution references in a data structure.

    Args:
        data: YAML data structure (dict, list, or scalar)
        prefix: Prefix to add to substitution names
        exclude: Set of substitution names to exclude from prefixing

    Returns:
        Data structure with prefixed substitution references
    """
    if exclude is None:
        exclude = set()

    def replace_sub(text: str) -> str:
        """Replace substitution references in a string."""

        def replace_match(match):
            sub_name = match.group(1)
            if sub_name in exclude:
                return match.group(0)
            # Always use braced format in output for consistency
            return f"${{{prefix}_{sub_name}}}"

        # Match both ${substitution} and $substitution formats
        return re.sub(r"\$\{?(\w+)\}?", replace_match, text)

    if isinstance(data, dict):
        result = {}
        for key, value in data.items():
            result[key] = prefix_substitutions_in_dict(value, prefix, exclude)
        return result
    if isinstance(data, list):
        return [prefix_substitutions_in_dict(item, prefix, exclude) for item in data]
    if isinstance(data, str):
        return replace_sub(data)
    return data


def deduplicate_by_id(data: dict) -> dict:
    """Deduplicate list items with the same ID.

    Keeps only the first occurrence of each ID. If items with the same ID
    are identical, this silently deduplicates. If they differ, the first
    one is kept (ESPHome's validation will catch if this causes issues).

    Args:
        data: Parsed config dictionary

    Returns:
        Config with deduplicated lists
    """
    if not isinstance(data, dict):
        return data

    result = {}
    for key, value in data.items():
        if isinstance(value, list):
            # Check for items with 'id' field
            seen_ids = set()
            deduped_list = []

            for item in value:
                if isinstance(item, dict) and "id" in item:
                    item_id = item["id"]
                    if item_id not in seen_ids:
                        seen_ids.add(item_id)
                        deduped_list.append(item)
                    # else: skip duplicate ID (keep first occurrence)
                else:
                    # No ID, just add it
                    deduped_list.append(item)

            result[key] = deduped_list
        elif isinstance(value, dict):
            # Recursively deduplicate nested dicts
            result[key] = deduplicate_by_id(value)
        else:
            result[key] = value

    return result


def merge_component_configs(
    component_names: list[str],
    platform: str,
    tests_dir: Path,
    output_file: Path,
) -> None:
    """Merge multiple component test configs into a single file.

    Args:
        component_names: List of component names to merge
        platform: Platform to merge for (e.g., "esp32-ard")
        tests_dir: Path to tests/components directory
        output_file: Path to output merged config file
    """
    if not component_names:
        raise ValueError("No components specified")

    # Track packages to ensure they're identical
    all_packages = None

    # Start with empty config
    merged_config_data = {}

    # Convert tests_dir to string for caching
    tests_dir_str = str(tests_dir)

    # Process each component
    for comp_name in component_names:
        comp_dir = tests_dir / comp_name
        test_file = comp_dir / f"test.{platform}.yaml"

        if not test_file.exists():
            raise FileNotFoundError(f"Test file not found: {test_file}")

        # Load the component's test file
        comp_data = load_yaml_file(test_file)

        # Merge packages from all components (cross-bus merging)
        # Components can have different packages (e.g., one with ble, another with uart)
        # as long as they don't conflict (checked by are_buses_compatible before calling this)
        comp_packages = extract_packages_from_yaml(comp_data)

        if all_packages is None:
            # First component - initialize package dict
            all_packages = comp_packages if comp_packages else {}
        elif comp_packages:
            # Merge packages - combine all unique package types
            # If both have the same package type, verify they're identical
            for pkg_name, pkg_config in comp_packages.items():
                if pkg_name in all_packages:
                    # Same package type - verify config matches
                    if all_packages[pkg_name] != pkg_config:
                        raise ValueError(
                            f"Component {comp_name} has conflicting config for package '{pkg_name}'. "
                            f"Expected: {all_packages[pkg_name]}, Got: {pkg_config}. "
                            f"Components with conflicting bus configs cannot be merged."
                        )
                else:
                    # New package type - add it
                    all_packages[pkg_name] = pkg_config

        # Handle $component_dir by replacing with absolute path
        # This allows components that use local file references to be grouped
        comp_abs_dir = str(comp_dir.absolute())

        # Save top-level substitutions BEFORE expanding packages
        # In ESPHome, top-level substitutions override package substitutions
        top_level_subs = (
            comp_data["substitutions"].copy()
            if "substitutions" in comp_data and comp_data["substitutions"] is not None
            else {}
        )

        # Expand packages - but we'll restore substitution priority after
        if "packages" in comp_data:
            packages_value = comp_data["packages"]

            if isinstance(packages_value, dict):
                # Dict format - check each package
                common_bus_packages = get_common_bus_packages()
                for pkg_name, pkg_value in list(packages_value.items()):
                    if pkg_name in common_bus_packages:
                        continue
                    if not isinstance(pkg_value, dict):
                        continue
                    # Component-specific package - expand its content into top level
                    comp_data = merge_config(comp_data, pkg_value)
            elif isinstance(packages_value, list):
                # List format - expand all package includes
                for pkg_value in packages_value:
                    if not isinstance(pkg_value, dict):
                        continue
                    comp_data = merge_config(comp_data, pkg_value)

            # Remove all packages (common will be re-added at the end)
            del comp_data["packages"]

        # Restore top-level substitution priority
        # Top-level substitutions override any from packages
        if "substitutions" not in comp_data or comp_data["substitutions"] is None:
            comp_data["substitutions"] = {}

        # Merge: package subs as base, top-level subs override
        comp_data["substitutions"].update(top_level_subs)

        # Now prefix the final merged substitutions
        comp_data["substitutions"] = {
            f"{comp_name}_{sub_name}": sub_value
            for sub_name, sub_value in comp_data["substitutions"].items()
        }

        # Add component_dir substitution with absolute path for this component
        comp_data["substitutions"][f"{comp_name}_component_dir"] = comp_abs_dir

        # Prefix substitution references throughout the config
        comp_data = prefix_substitutions_in_dict(comp_data, comp_name)

        # Use ESPHome's merge_config to merge this component into the result
        # merge_config handles list merging with ID-based deduplication automatically
        merged_config_data = merge_config(merged_config_data, comp_data)

    # Add merged packages back (union of all component packages)
    # IMPORTANT: Only include common bus packages (spi, i2c, uart, etc.)
    # Do NOT re-add component-specific packages as they contain unprefixed $component_dir refs
    if all_packages:
        # Build packages dict from merged all_packages
        # all_packages is a dict mapping package_name -> str(package_value)
        # We need to reconstruct the actual package values by loading them from any component
        # Since packages with the same name must have identical configs (verified above),
        # we can load the package value from the first component that has each package
        common_bus_packages = get_common_bus_packages()
        merged_packages: dict[str, Any] = {}

        # Collect packages that are included as dependencies
        # If modbus is present, uart is included via modbus.packages.uart
        packages_to_skip: set[str] = set()
        for pkg_name in all_packages:
            if pkg_name.startswith(DEPENDENCY_MARKER_PREFIX):
                # Extract the actual package name (remove _dep_ prefix)
                dep_name = pkg_name[len(DEPENDENCY_MARKER_PREFIX) :]
                packages_to_skip.add(dep_name)

        for pkg_name in all_packages:
            # Skip dependency markers
            if pkg_name.startswith(DEPENDENCY_MARKER_PREFIX):
                continue
            # Skip non-common-bus packages
            if pkg_name not in common_bus_packages:
                continue
            # Skip packages that are included as dependencies of other packages
            # This prevents duplicate definitions (e.g., uart via modbus + uart separately)
            if pkg_name in packages_to_skip:
                continue

            # Find a component that has this package and extract its value
            # Uses cached lookup to avoid re-loading the same files
            for comp_name in component_names:
                comp_packages = get_component_packages(
                    comp_name, platform, tests_dir_str
                )
                if pkg_name in comp_packages:
                    merged_packages[pkg_name] = comp_packages[pkg_name]
                    break

        if merged_packages:
            merged_config_data["packages"] = merged_packages

    # Deduplicate items with same ID (keeps first occurrence)
    merged_config_data = deduplicate_by_id(merged_config_data)

    # Remove esphome section since it will be provided by the wrapper file
    # The wrapper file includes this merged config via packages and provides
    # the proper esphome: section with name, platform, etc.
    if "esphome" in merged_config_data:
        del merged_config_data["esphome"]

    # Write merged config
    output_file.parent.mkdir(parents=True, exist_ok=True)
    yaml_content = yaml_util.dump(merged_config_data)
    output_file.write_text(yaml_content)

    print(f"Successfully merged {len(component_names)} components into {output_file}")


def main() -> None:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Merge multiple component test configs into a single file"
    )
    parser.add_argument(
        "--components",
        "-c",
        required=True,
        help="Comma-separated list of component names to merge",
    )
    parser.add_argument(
        "--platform",
        "-p",
        required=True,
        help="Platform to merge for (e.g., esp32-ard)",
    )
    parser.add_argument(
        "--output",
        "-o",
        required=True,
        type=Path,
        help="Output file path for merged config",
    )
    parser.add_argument(
        "--tests-dir",
        type=Path,
        default=Path("tests/components"),
        help="Path to tests/components directory",
    )

    args = parser.parse_args()

    component_names = [c.strip() for c in args.components.split(",")]

    try:
        merge_component_configs(
            component_names=component_names,
            platform=args.platform,
            tests_dir=args.tests_dir,
            output_file=args.output,
        )
    except Exception as e:
        print(f"Error merging configs: {e}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
