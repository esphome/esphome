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
from pathlib import Path
import re
import sys
from typing import Any

# Add esphome to path so we can import from it
sys.path.insert(0, str(Path(__file__).parent.parent))

from esphome import yaml_util
from esphome.config_helpers import merge_config
from script.analyze_component_buses import PACKAGE_DEPENDENCIES, get_common_bus_packages


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
            packages[f"_dep_{dep}"] = f"(included via {name})"

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

    # Process each component
    for comp_name in component_names:
        comp_dir = tests_dir / comp_name
        test_file = comp_dir / f"test.{platform}.yaml"

        if not test_file.exists():
            raise FileNotFoundError(f"Test file not found: {test_file}")

        # Load the component's test file
        comp_data = load_yaml_file(test_file)

        # Validate packages are compatible
        # Components with no packages (no_buses) can merge with any group
        comp_packages = extract_packages_from_yaml(comp_data)

        if all_packages is None:
            # First component - set the baseline
            all_packages = comp_packages
        elif not comp_packages:
            # This component has no packages (no_buses) - it can merge with any group
            pass
        elif not all_packages:
            # Previous components had no packages, but this one does - adopt these packages
            all_packages = comp_packages
        elif comp_packages != all_packages:
            # Both have packages but they differ - this is an error
            raise ValueError(
                f"Component {comp_name} has different packages than previous components. "
                f"Expected: {all_packages}, Got: {comp_packages}. "
                f"All components must use the same common bus configs to be merged."
            )

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

    # Add packages back (only once, since they're identical)
    # IMPORTANT: Only re-add common bus packages (spi, i2c, uart, etc.)
    # Do NOT re-add component-specific packages as they contain unprefixed $component_dir refs
    if all_packages:
        first_comp_data = load_yaml_file(
            tests_dir / component_names[0] / f"test.{platform}.yaml"
        )
        if "packages" in first_comp_data and isinstance(
            first_comp_data["packages"], dict
        ):
            # Filter to only include common bus packages
            # Only dict format can contain common bus packages
            common_bus_packages = get_common_bus_packages()
            filtered_packages = {
                name: value
                for name, value in first_comp_data["packages"].items()
                if name in common_bus_packages
            }
            if filtered_packages:
                merged_config_data["packages"] = filtered_packages

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
