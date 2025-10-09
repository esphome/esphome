#!/usr/bin/env python3
"""Test ESPHome component builds with intelligent grouping.

This script replaces the bash test_build_components script with Python,
adding support for intelligent component grouping based on shared bus
configurations to reduce CI build time.

Features:
- Analyzes components for shared common bus configs
- Groups compatible components together
- Merges configs for grouped components
- Uses --testing-mode for grouped tests
- Maintains backward compatibility with single component testing
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
import subprocess
import sys

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from script.analyze_component_buses import (
    analyze_all_components,
    create_grouping_signature,
)
from script.merge_component_configs import merge_component_configs


def find_component_tests(
    components_dir: Path, component_pattern: str = "*"
) -> dict[str, list[Path]]:
    """Find all component test files.

    Args:
        components_dir: Path to tests/components directory
        component_pattern: Glob pattern for component names

    Returns:
        Dictionary mapping component name to list of test files
    """
    component_tests = defaultdict(list)

    for comp_dir in components_dir.glob(component_pattern):
        if not comp_dir.is_dir():
            continue

        for test_file in comp_dir.glob("test.*.yaml"):
            component_tests[comp_dir.name].append(test_file)

    return dict(component_tests)


def parse_test_filename(test_file: Path) -> tuple[str, str]:
    """Parse test filename to extract test name and platform.

    Args:
        test_file: Path to test file

    Returns:
        Tuple of (test_name, platform)
    """
    parts = test_file.stem.split(".")
    if len(parts) == 2:
        return parts[0], parts[1]  # test, platform
    return parts[0], "all"


def get_platform_base_files(base_dir: Path) -> dict[str, list[Path]]:
    """Get all platform base files.

    Args:
        base_dir: Path to test_build_components directory

    Returns:
        Dictionary mapping platform to list of base files (for version variants)
    """
    platform_files = defaultdict(list)

    for base_file in base_dir.glob("build_components_base.*.yaml"):
        # Extract platform from filename
        # e.g., build_components_base.esp32-idf.yaml -> esp32-idf
        # or build_components_base.esp32-idf-50.yaml -> esp32-idf
        filename = base_file.stem
        parts = filename.replace("build_components_base.", "").split("-")

        # Platform is everything before version number (if present)
        # Check if last part is a number (version)
        platform = "-".join(parts[:-1]) if parts[-1].isdigit() else "-".join(parts)

        platform_files[platform].append(base_file)

    return dict(platform_files)


def extract_platform_with_version(base_file: Path) -> str:
    """Extract platform with version from base filename.

    Args:
        base_file: Path to base file

    Returns:
        Platform with version (e.g., "esp32-idf-50" or "esp32-idf")
    """
    # Remove "build_components_base." prefix and ".yaml" suffix
    return base_file.stem.replace("build_components_base.", "")


def run_esphome_test(
    component: str,
    test_file: Path,
    platform: str,
    platform_with_version: str,
    base_file: Path,
    build_dir: Path,
    esphome_command: str,
    continue_on_fail: bool,
    use_testing_mode: bool = False,
) -> bool:
    """Run esphome test for a single component.

    Args:
        component: Component name
        test_file: Path to component test file
        platform: Platform name (e.g., "esp32-idf")
        platform_with_version: Platform with version (e.g., "esp32-idf-50")
        base_file: Path to platform base file
        build_dir: Path to build directory
        esphome_command: ESPHome command (config/compile)
        continue_on_fail: Whether to continue on failure
        use_testing_mode: Whether to use --testing-mode flag

    Returns:
        True if test passed, False otherwise
    """
    test_name = test_file.stem.split(".")[0]

    # Create dynamic test file in build directory
    output_file = build_dir / f"{component}.{test_name}.{platform_with_version}.yaml"

    # Copy base file and substitute component test file reference
    base_content = base_file.read_text()
    # Get relative path from build dir to test file
    repo_root = Path(__file__).parent.parent
    component_test_ref = f"../../{test_file.relative_to(repo_root / 'tests')}"
    output_content = base_content.replace("$component_test_file", component_test_ref)
    output_file.write_text(output_content)

    # Build esphome command
    cmd = [
        sys.executable,
        "-m",
        "esphome",
    ]

    # Add --testing-mode if needed (must be before subcommand)
    if use_testing_mode:
        cmd.append("--testing-mode")

    # Add substitutions
    cmd.extend(
        [
            "-s",
            "component_name",
            component,
            "-s",
            "component_dir",
            f"../../components/{component}",
            "-s",
            "test_name",
            test_name,
            "-s",
            "target_platform",
            platform,
        ]
    )

    # Add command and config file
    cmd.extend([esphome_command, str(output_file)])

    # Run command
    print(f"> [{component}] [{test_name}] [{platform_with_version}]")
    if use_testing_mode:
        print("  (using --testing-mode)")

    try:
        result = subprocess.run(cmd, check=not continue_on_fail)
        return result.returncode == 0
    except subprocess.CalledProcessError:
        if not continue_on_fail:
            raise
        return False


def run_grouped_test(
    components: list[str],
    platform: str,
    platform_with_version: str,
    base_file: Path,
    build_dir: Path,
    tests_dir: Path,
    esphome_command: str,
    continue_on_fail: bool,
) -> bool:
    """Run esphome test for a group of components with shared bus configs.

    Args:
        components: List of component names to test together
        platform: Platform name (e.g., "esp32-idf")
        platform_with_version: Platform with version (e.g., "esp32-idf-50")
        base_file: Path to platform base file
        build_dir: Path to build directory
        tests_dir: Path to tests/components directory
        esphome_command: ESPHome command (config/compile)
        continue_on_fail: Whether to continue on failure

    Returns:
        True if test passed, False otherwise
    """
    # Create merged config
    group_name = "_".join(components[:3])  # Use first 3 components for name
    if len(components) > 3:
        group_name += f"_plus_{len(components) - 3}"

    merged_config_file = build_dir / f"merged_{group_name}.{platform_with_version}.yaml"

    try:
        merge_component_configs(
            component_names=components,
            platform=platform_with_version,
            tests_dir=tests_dir,
            output_file=merged_config_file,
        )
    except Exception as e:
        print(f"Error merging configs for {components}: {e}")
        if not continue_on_fail:
            raise
        return False

    # Create test file that includes merged config
    output_file = build_dir / f"test_{group_name}.{platform_with_version}.yaml"
    base_content = base_file.read_text()
    merged_ref = merged_config_file.name
    output_content = base_content.replace("$component_test_file", merged_ref)
    output_file.write_text(output_content)

    # Build esphome command with --testing-mode
    cmd = [
        sys.executable,
        "-m",
        "esphome",
        "--testing-mode",  # Required for grouped tests
        "-s",
        "component_name",
        group_name,
        "-s",
        "component_dir",
        "../../components",
        "-s",
        "test_name",
        "merged",
        "-s",
        "target_platform",
        platform,
        esphome_command,
        str(output_file),
    ]

    # Run command
    components_str = ", ".join(components)
    print(f"> [GROUPED: {components_str}] [{platform_with_version}]")
    print("  (using --testing-mode)")

    try:
        result = subprocess.run(cmd, check=not continue_on_fail)
        return result.returncode == 0
    except subprocess.CalledProcessError:
        if not continue_on_fail:
            raise
        return False


def run_grouped_component_tests(
    all_tests: dict[str, list[Path]],
    platform_filter: str | None,
    platform_bases: dict[str, list[Path]],
    tests_dir: Path,
    build_dir: Path,
    esphome_command: str,
    continue_on_fail: bool,
) -> tuple[set[tuple[str, str]], list[str], list[str]]:
    """Run grouped component tests.

    Args:
        all_tests: Dictionary mapping component names to test files
        platform_filter: Optional platform to filter by
        platform_bases: Platform base files mapping
        tests_dir: Path to tests/components directory
        build_dir: Path to build directory
        esphome_command: ESPHome command (config/compile)
        continue_on_fail: Whether to continue on failure

    Returns:
        Tuple of (tested_components, passed_tests, failed_tests)
    """
    tested_components = set()
    passed_tests = []
    failed_tests = []

    # Group components by platform and bus signature
    grouped_components: dict[tuple[str, str], list[str]] = defaultdict(list)
    print("\n" + "=" * 80)
    print("Analyzing components for intelligent grouping...")
    print("=" * 80)
    component_buses, non_groupable = analyze_all_components(tests_dir)

    # Group by (platform, bus_signature)
    for component, platforms in component_buses.items():
        if component not in all_tests:
            continue

        # Skip components that use local file references
        if component in non_groupable:
            continue

        for platform, buses in platforms.items():
            # Skip if platform doesn't match filter
            if platform_filter and not platform.startswith(platform_filter):
                continue

            # Only group if component has common bus configs
            if buses:
                signature = create_grouping_signature({platform: buses}, platform)
                grouped_components[(platform, signature)].append(component)

    # Print detailed grouping plan
    print("\nGrouping Plan:")
    print("-" * 80)

    # Show excluded components
    if non_groupable:
        excluded_in_tests = [c for c in non_groupable if c in all_tests]
        if excluded_in_tests:
            print(
                f"\n⚠ {len(excluded_in_tests)} components excluded from grouping (use local file references):"
            )
            for comp in sorted(excluded_in_tests[:5]):
                print(f"  - {comp}")
            if len(excluded_in_tests) > 5:
                print(f"  ... and {len(excluded_in_tests) - 5} more")

    groups_to_test = []
    individual_tests = []

    for (platform, signature), components in sorted(grouped_components.items()):
        if len(components) > 1:
            groups_to_test.append((platform, signature, components))
        elif len(components) == 1:
            individual_tests.extend(components)

    # Add components without grouping signatures
    for component in all_tests:
        if (
            component not in [c for _, _, comps in groups_to_test for c in comps]
            and component not in individual_tests
        ):
            individual_tests.append(component)

    if groups_to_test:
        print(f"\n✓ {len(groups_to_test)} groups will be tested together:")
        for platform, signature, components in groups_to_test:
            component_list = ", ".join(sorted(components))
            print(f"  [{platform}] [{signature}]: {component_list}")
            print(
                f"    → {len(components)} components in 1 build (saves {len(components) - 1} builds)"
            )

    if individual_tests:
        print(f"\n○ {len(individual_tests)} components will be tested individually:")
        for comp in sorted(individual_tests[:10]):
            print(f"  - {comp}")
        if len(individual_tests) > 10:
            print(f"  ... and {len(individual_tests) - 10} more")

    total_grouped = sum(len(comps) for _, _, comps in groups_to_test)
    total_builds_without_grouping = len(all_tests)
    total_builds_with_grouping = len(groups_to_test) + len(individual_tests)
    builds_saved = total_builds_without_grouping - total_builds_with_grouping

    print(f"\n{'=' * 80}")
    print(f"Summary: {total_builds_with_grouping} builds total")
    print(f"  • {len(groups_to_test)} grouped builds ({total_grouped} components)")
    print(f"  • {len(individual_tests)} individual builds")
    print(
        f"  • Saves {builds_saved} builds ({builds_saved / total_builds_without_grouping * 100:.1f}% reduction)"
    )
    print("=" * 80 + "\n")

    # Execute grouped tests
    for (platform, signature), components in grouped_components.items():
        # Only group if we have multiple components with same signature
        if len(components) <= 1:
            continue

        # Filter out components not in our test list
        components_to_group = [c for c in components if c in all_tests]
        if len(components_to_group) <= 1:
            continue

        # Get platform base files
        if platform not in platform_bases:
            continue

        for base_file in platform_bases[platform]:
            platform_with_version = extract_platform_with_version(base_file)

            # Skip if platform filter doesn't match
            if platform_filter and platform != platform_filter:
                continue
            if (
                platform_filter
                and platform_with_version != platform_filter
                and not platform_with_version.startswith(f"{platform_filter}-")
            ):
                continue

            # Run grouped test
            success = run_grouped_test(
                components=components_to_group,
                platform=platform,
                platform_with_version=platform_with_version,
                base_file=base_file,
                build_dir=build_dir,
                tests_dir=tests_dir,
                esphome_command=esphome_command,
                continue_on_fail=continue_on_fail,
            )

            # Mark all components as tested
            for comp in components_to_group:
                tested_components.add((comp, platform_with_version))

            # Record result for each component - show all components in grouped tests
            test_id = (
                f"GROUPED[{','.join(components_to_group)}].{platform_with_version}"
            )
            if success:
                passed_tests.append(test_id)
            else:
                failed_tests.append(test_id)

    return tested_components, passed_tests, failed_tests


def test_components(
    component_patterns: list[str],
    platform_filter: str | None,
    esphome_command: str,
    continue_on_fail: bool,
    enable_grouping: bool = True,
) -> int:
    """Test components with optional intelligent grouping.

    Args:
        component_patterns: List of component name patterns
        platform_filter: Optional platform to filter by
        esphome_command: ESPHome command (config/compile)
        continue_on_fail: Whether to continue on failure
        enable_grouping: Whether to enable component grouping

    Returns:
        Exit code (0 for success, 1 for failure)
    """
    # Setup paths
    repo_root = Path(__file__).parent.parent
    tests_dir = repo_root / "tests" / "components"
    build_components_dir = repo_root / "tests" / "test_build_components"
    build_dir = build_components_dir / "build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Get platform base files
    platform_bases = get_platform_base_files(build_components_dir)

    # Find all component tests
    all_tests = {}
    for pattern in component_patterns:
        all_tests.update(find_component_tests(tests_dir, pattern))

    if not all_tests:
        print(f"No components found matching: {component_patterns}")
        return 1

    print(f"Found {len(all_tests)} components to test")

    # Run tests
    failed_tests = []
    passed_tests = []
    tested_components = set()  # Track which components were tested in groups

    # First, run grouped tests if grouping is enabled
    if enable_grouping:
        tested_components, passed_tests, failed_tests = run_grouped_component_tests(
            all_tests=all_tests,
            platform_filter=platform_filter,
            platform_bases=platform_bases,
            tests_dir=tests_dir,
            build_dir=build_dir,
            esphome_command=esphome_command,
            continue_on_fail=continue_on_fail,
        )

    # Then run individual tests for components not in groups
    for component, test_files in sorted(all_tests.items()):
        for test_file in test_files:
            test_name, platform = parse_test_filename(test_file)

            # Handle "all" platform tests
            if platform == "all":
                # Run for all platforms
                for plat, base_files in platform_bases.items():
                    if platform_filter and plat != platform_filter:
                        continue

                    for base_file in base_files:
                        platform_with_version = extract_platform_with_version(base_file)

                        # Skip if already tested in a group
                        if (component, platform_with_version) in tested_components:
                            continue

                        success = run_esphome_test(
                            component=component,
                            test_file=test_file,
                            platform=plat,
                            platform_with_version=platform_with_version,
                            base_file=base_file,
                            build_dir=build_dir,
                            esphome_command=esphome_command,
                            continue_on_fail=continue_on_fail,
                        )
                        test_id = f"{component}.{test_name}.{platform_with_version}"
                        if success:
                            passed_tests.append(test_id)
                        else:
                            failed_tests.append(test_id)
            else:
                # Platform-specific test
                if platform_filter and platform != platform_filter:
                    continue

                if platform not in platform_bases:
                    print(f"No base file for platform: {platform}")
                    continue

                for base_file in platform_bases[platform]:
                    platform_with_version = extract_platform_with_version(base_file)

                    # Skip if requested platform doesn't match
                    if (
                        platform_filter
                        and platform_with_version != platform_filter
                        and not platform_with_version.startswith(f"{platform_filter}-")
                    ):
                        continue

                    # Skip if already tested in a group
                    if (component, platform_with_version) in tested_components:
                        continue

                    success = run_esphome_test(
                        component=component,
                        test_file=test_file,
                        platform=platform,
                        platform_with_version=platform_with_version,
                        base_file=base_file,
                        build_dir=build_dir,
                        esphome_command=esphome_command,
                        continue_on_fail=continue_on_fail,
                    )
                    test_id = f"{component}.{test_name}.{platform_with_version}"
                    if success:
                        passed_tests.append(test_id)
                    else:
                        failed_tests.append(test_id)

    # Print summary
    print("\n" + "=" * 80)
    print(f"Test Summary: {len(passed_tests)} passed, {len(failed_tests)} failed")
    print("=" * 80)

    if failed_tests:
        print("\nFailed tests:")
        for test in failed_tests:
            print(f"  - {test}")
        return 1

    return 0


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Test ESPHome component builds with intelligent grouping"
    )
    parser.add_argument(
        "-e",
        "--esphome-command",
        default="compile",
        choices=["config", "compile", "clean"],
        help="ESPHome command to run (default: compile)",
    )
    parser.add_argument(
        "-c",
        "--components",
        default="*",
        help="Component pattern(s) to test (default: *). Comma-separated.",
    )
    parser.add_argument(
        "-t",
        "--target",
        help="Target platform to test (e.g., esp32-idf)",
    )
    parser.add_argument(
        "-f",
        "--continue-on-fail",
        action="store_true",
        help="Continue testing even if a test fails",
    )
    parser.add_argument(
        "--no-grouping",
        action="store_true",
        help="Disable component grouping (test each component individually)",
    )

    args = parser.parse_args()

    # Parse component patterns
    component_patterns = [p.strip() for p in args.components.split(",")]

    return test_components(
        component_patterns=component_patterns,
        platform_filter=args.target,
        esphome_command=args.esphome_command,
        continue_on_fail=args.continue_on_fail,
        enable_grouping=not args.no_grouping,
    )


if __name__ == "__main__":
    sys.exit(main())
