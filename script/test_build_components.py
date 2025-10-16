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
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import time

# Add esphome to path
sys.path.insert(0, str(Path(__file__).parent.parent))

# pylint: disable=wrong-import-position
from script.analyze_component_buses import (
    BASE_BUS_COMPONENTS,
    ISOLATED_COMPONENTS,
    NO_BUSES_SIGNATURE,
    analyze_all_components,
    create_grouping_signature,
    is_platform_component,
    merge_compatible_bus_groups,
    uses_local_file_references,
)
from script.merge_component_configs import merge_component_configs


@dataclass
class TestResult:
    """Store information about a single test run."""

    test_id: str
    components: list[str]
    platform: str
    success: bool
    duration: float
    command: str = ""
    test_type: str = "compile"  # "config" or "compile"


def show_disk_space_if_ci(esphome_command: str) -> None:
    """Show disk space usage if running in CI during compile.

    Only shows output during compilation (not config validation) since
    disk space is only relevant when actually building firmware.

    Args:
        esphome_command: The esphome command being run (config/compile/clean)
    """
    # Only show disk space during compilation in CI
    # Config validation doesn't build anything so disk space isn't relevant
    if not os.environ.get("GITHUB_ACTIONS"):
        return
    if esphome_command != "compile":
        return

    print("\n" + "=" * 80)
    print("Disk Space After Build:")
    print("=" * 80)
    # Use sys.stdout.flush() to ensure output appears immediately
    sys.stdout.flush()
    subprocess.run(["df", "-h"], check=False, stdout=sys.stdout, stderr=sys.stderr)
    print("=" * 80 + "\n")
    sys.stdout.flush()


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


def group_components_by_platform(
    failed_results: list[TestResult],
) -> dict[tuple[str, str], list[str]]:
    """Group failed components by platform and test type for simplified reproduction commands.

    Args:
        failed_results: List of failed test results

    Returns:
        Dictionary mapping (platform, test_type) to list of component names
    """
    platform_components: dict[tuple[str, str], list[str]] = {}
    for result in failed_results:
        key = (result.platform, result.test_type)
        if key not in platform_components:
            platform_components[key] = []
        platform_components[key].extend(result.components)

    # Remove duplicates and sort for each platform
    return {
        key: sorted(set(components)) for key, components in platform_components.items()
    }


def format_github_summary(test_results: list[TestResult]) -> str:
    """Format test results as GitHub Actions job summary markdown.

    Args:
        test_results: List of all test results

    Returns:
        Markdown formatted summary string
    """
    # Separate results into passed and failed
    passed_results = [r for r in test_results if r.success]
    failed_results = [r for r in test_results if not r.success]

    lines = []

    # Header with emoji based on success/failure
    if failed_results:
        lines.append("## :x: Component Tests Failed\n")
    else:
        lines.append("## :white_check_mark: Component Tests Passed\n")

    # Summary statistics
    total_time = sum(r.duration for r in test_results)
    # Determine test type from results (all should be the same)
    test_type = test_results[0].test_type if test_results else "unknown"
    lines.append(
        f"**Results:** {len(passed_results)} passed, {len(failed_results)} failed\n"
    )
    lines.append(f"**Total time:** {total_time:.1f}s\n")
    lines.append(f"**Test type:** `{test_type}`\n")

    # Show failed tests if any
    if failed_results:
        lines.append("### Failed Tests\n")
        lines.append("| Test | Components | Platform | Duration |\n")
        lines.append("|------|-----------|----------|----------|\n")
        for result in failed_results:
            components_str = ", ".join(result.components)
            lines.append(
                f"| `{result.test_id}` | {components_str} | {result.platform} | {result.duration:.1f}s |\n"
            )
        lines.append("\n")

        # Show simplified commands to reproduce failures
        # Group all failed components by platform for a single command per platform
        lines.append("<details>\n")
        lines.append("<summary>Commands to reproduce failures</summary>\n\n")
        lines.append("```bash\n")

        # Generate one command per platform and test type
        platform_components = group_components_by_platform(failed_results)
        for platform, test_type in sorted(platform_components.keys()):
            components_csv = ",".join(platform_components[(platform, test_type)])
            lines.append(
                f"script/test_build_components.py -c {components_csv} -t {platform} -e {test_type}\n"
            )

        lines.append("```\n")
        lines.append("</details>\n")

    # Show passed tests
    if passed_results:
        lines.append("### Passed Tests\n\n")
        lines.append(f"{len(passed_results)} tests passed successfully\n")

        # Separate grouped and individual tests
        grouped_results = [r for r in passed_results if len(r.components) > 1]
        individual_results = [r for r in passed_results if len(r.components) == 1]

        if grouped_results:
            lines.append("#### Grouped Tests\n")
            lines.append("| Components | Platform | Count | Duration |\n")
            lines.append("|-----------|----------|-------|----------|\n")
            for result in grouped_results:
                components_str = ", ".join(result.components)
                lines.append(
                    f"| {components_str} | {result.platform} | {len(result.components)} | {result.duration:.1f}s |\n"
                )
            lines.append("\n")

        if individual_results:
            lines.append("#### Individual Tests\n")
            # Show first 10 individual tests with timing
            if len(individual_results) <= 10:
                lines.extend(
                    f"- `{result.test_id}` - {result.duration:.1f}s\n"
                    for result in individual_results
                )
            else:
                lines.extend(
                    f"- `{result.test_id}` - {result.duration:.1f}s\n"
                    for result in individual_results[:10]
                )
                lines.append(f"\n...and {len(individual_results) - 10} more\n")
            lines.append("\n")

    return "".join(lines)


def write_github_summary(test_results: list[TestResult]) -> None:
    """Write GitHub Actions job summary with test results and timing.

    Args:
        test_results: List of all test results
    """
    summary_content = format_github_summary(test_results)
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as f:
        f.write(summary_content)


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
) -> TestResult:
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
        TestResult object with test details and timing
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

    # Build command string for display/logging
    cmd_str = " ".join(cmd)

    # Run command
    print(f"> [{component}] [{test_name}] [{platform_with_version}]")
    if use_testing_mode:
        print("  (using --testing-mode)")

    start_time = time.time()
    test_id = f"{component}.{test_name}.{platform_with_version}"

    try:
        result = subprocess.run(cmd, check=False)
        success = result.returncode == 0
        duration = time.time() - start_time

        # Show disk space after build in CI during compile
        show_disk_space_if_ci(esphome_command)

        if not success and not continue_on_fail:
            # Print command immediately for failed tests
            print(f"\n{'=' * 80}")
            print("FAILED - Command to reproduce:")
            print(f"{'=' * 80}")
            print(cmd_str)
            print()
            raise subprocess.CalledProcessError(result.returncode, cmd)

        return TestResult(
            test_id=test_id,
            components=[component],
            platform=platform_with_version,
            success=success,
            duration=duration,
            command=cmd_str,
            test_type=esphome_command,
        )
    except subprocess.CalledProcessError:
        duration = time.time() - start_time
        # Re-raise if we're not continuing on fail
        if not continue_on_fail:
            raise
        return TestResult(
            test_id=test_id,
            components=[component],
            platform=platform_with_version,
            success=False,
            duration=duration,
            command=cmd_str,
            test_type=esphome_command,
        )


def run_grouped_test(
    components: list[str],
    platform: str,
    platform_with_version: str,
    base_file: Path,
    build_dir: Path,
    tests_dir: Path,
    esphome_command: str,
    continue_on_fail: bool,
) -> TestResult:
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
        TestResult object with test details and timing
    """
    # Create merged config
    group_name = "_".join(components[:3])  # Use first 3 components for name
    if len(components) > 3:
        group_name += f"_plus_{len(components) - 3}"

    # Create unique device name by hashing sorted component list + platform
    # This prevents conflicts when different component groups are tested
    sorted_components = sorted(components)
    hash_input = "_".join(sorted_components) + "_" + platform
    group_hash = hashlib.md5(hash_input.encode()).hexdigest()[:8]
    device_name = f"comptest{platform.replace('-', '')}{group_hash}"

    merged_config_file = build_dir / f"merged_{group_name}.{platform_with_version}.yaml"

    try:
        merge_component_configs(
            component_names=components,
            platform=platform_with_version,
            tests_dir=tests_dir,
            output_file=merged_config_file,
        )
    except Exception as e:  # pylint: disable=broad-exception-caught
        print(f"Error merging configs for {components}: {e}")
        if not continue_on_fail:
            raise
        # Return TestResult for merge failure
        test_id = f"GROUPED[{','.join(components)}].{platform_with_version}"
        return TestResult(
            test_id=test_id,
            components=components,
            platform=platform_with_version,
            success=False,
            duration=0.0,
            command=f"# Failed during config merge: {e}",
            test_type=esphome_command,
        )

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
        device_name,  # Use unique hash-based device name
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

    # Build command string for display/logging
    cmd_str = " ".join(cmd)

    # Run command
    components_str = ", ".join(components)
    print(f"> [GROUPED: {components_str}] [{platform_with_version}]")
    print("  (using --testing-mode)")

    start_time = time.time()
    test_id = f"GROUPED[{','.join(components)}].{platform_with_version}"

    try:
        result = subprocess.run(cmd, check=False)
        success = result.returncode == 0
        duration = time.time() - start_time

        # Show disk space after build in CI during compile
        show_disk_space_if_ci(esphome_command)

        if not success and not continue_on_fail:
            # Print command immediately for failed tests
            print(f"\n{'=' * 80}")
            print("FAILED - Command to reproduce:")
            print(f"{'=' * 80}")
            print(cmd_str)
            print()
            raise subprocess.CalledProcessError(result.returncode, cmd)

        return TestResult(
            test_id=test_id,
            components=components,
            platform=platform_with_version,
            success=success,
            duration=duration,
            command=cmd_str,
            test_type=esphome_command,
        )
    except subprocess.CalledProcessError:
        duration = time.time() - start_time
        # Re-raise if we're not continuing on fail
        if not continue_on_fail:
            raise
        return TestResult(
            test_id=test_id,
            components=components,
            platform=platform_with_version,
            success=False,
            duration=duration,
            command=cmd_str,
            test_type=esphome_command,
        )


def run_grouped_component_tests(
    all_tests: dict[str, list[Path]],
    platform_filter: str | None,
    platform_bases: dict[str, list[Path]],
    tests_dir: Path,
    build_dir: Path,
    esphome_command: str,
    continue_on_fail: bool,
    additional_isolated: set[str] | None = None,
) -> tuple[set[tuple[str, str]], list[TestResult]]:
    """Run grouped component tests.

    Args:
        all_tests: Dictionary mapping component names to test files
        platform_filter: Optional platform to filter by
        platform_bases: Platform base files mapping
        tests_dir: Path to tests/components directory
        build_dir: Path to build directory
        esphome_command: ESPHome command (config/compile)
        continue_on_fail: Whether to continue on failure
        additional_isolated: Additional components to treat as isolated (not grouped)

    Returns:
        Tuple of (tested_components, test_results)
    """
    tested_components = set()
    test_results = []

    # Group components by platform and bus signature
    grouped_components: dict[tuple[str, str], list[str]] = defaultdict(list)
    print("\n" + "=" * 80)
    print("Analyzing components for intelligent grouping...")
    print("=" * 80)
    component_buses, non_groupable, direct_bus_components = analyze_all_components(
        tests_dir
    )

    # Track why components can't be grouped (for detailed output)
    non_groupable_reasons = {}

    # Merge additional isolated components with predefined ones
    # ISOLATED COMPONENTS are tested individually WITHOUT --testing-mode
    # This is critical because:
    # - Grouped tests use --testing-mode which disables pin conflict checks and other validation
    # - These checks are disabled to allow config merging (multiple components in one build)
    # - For directly changed components (via --isolate), we need full validation to catch issues
    # - Dependencies are safe to group since they weren't modified in the PR
    all_isolated = set(ISOLATED_COMPONENTS.keys())
    if additional_isolated:
        all_isolated.update(additional_isolated)

    # Group by (platform, bus_signature)
    for component, platforms in component_buses.items():
        if component not in all_tests:
            continue

        # Skip components that must be tested in isolation
        # These are shown separately and should not be in non_groupable_reasons
        if component in all_isolated:
            continue

        # Skip base bus components (these test the bus platforms themselves)
        if component in BASE_BUS_COMPONENTS:
            continue

        # Skip components that use local file references or direct bus configs
        if component in non_groupable:
            # Track the reason (using pre-calculated results to avoid expensive re-analysis)
            if component not in non_groupable_reasons:
                if component in direct_bus_components:
                    non_groupable_reasons[component] = (
                        "Defines buses directly (not via packages) - NEEDS MIGRATION"
                    )
                elif uses_local_file_references(tests_dir / component):
                    non_groupable_reasons[component] = (
                        "Uses local file references ($component_dir)"
                    )
                elif is_platform_component(tests_dir / component):
                    non_groupable_reasons[component] = (
                        "Platform component (abstract base class)"
                    )
                else:
                    non_groupable_reasons[component] = (
                        "Uses !extend or !remove directives"
                    )
            continue

        for platform, buses in platforms.items():
            # Skip if platform doesn't match filter
            if platform_filter and not platform.startswith(platform_filter):
                continue

            # Create signature for this component's bus configuration
            # Components with no buses get NO_BUSES_SIGNATURE so they can be grouped together
            if buses:
                signature = create_grouping_signature({platform: buses}, platform)
            else:
                signature = NO_BUSES_SIGNATURE

            # Add to grouped components (including those with no buses)
            if signature:
                grouped_components[(platform, signature)].append(component)

    # Merge groups with compatible buses (cross-bus grouping optimization)
    # This allows mixing components with different buses (e.g., ble + uart)
    # as long as they don't have conflicting configurations for the same bus type
    grouped_components = merge_compatible_bus_groups(grouped_components)

    # Print detailed grouping plan
    print("\nGrouping Plan:")
    print("-" * 80)

    # Show isolated components (must test individually due to known issues or direct changes)
    isolated_in_tests = [c for c in all_isolated if c in all_tests]
    if isolated_in_tests:
        predefined_isolated = [c for c in isolated_in_tests if c in ISOLATED_COMPONENTS]
        additional_in_tests = [
            c for c in isolated_in_tests if c in (additional_isolated or set())
        ]

        if predefined_isolated:
            print(
                f"\n⚠ {len(predefined_isolated)} components must be tested in isolation (known build issues):"
            )
            for comp in sorted(predefined_isolated):
                reason = ISOLATED_COMPONENTS[comp]
                print(f"  - {comp}: {reason}")

        if additional_in_tests:
            print(
                f"\n✓ {len(additional_in_tests)} components tested in isolation (directly changed in PR):"
            )
            for comp in sorted(additional_in_tests):
                print(f"  - {comp}")

    # Show base bus components (test the bus platform implementations)
    base_bus_in_tests = [c for c in BASE_BUS_COMPONENTS if c in all_tests]
    if base_bus_in_tests:
        print(
            f"\n○ {len(base_bus_in_tests)} base bus platform components (tested individually):"
        )
        for comp in sorted(base_bus_in_tests):
            print(f"  - {comp}")

    # Show excluded components with detailed reasons
    if non_groupable_reasons:
        excluded_in_tests = [c for c in non_groupable_reasons if c in all_tests]
        if excluded_in_tests:
            print(
                f"\n⚠ {len(excluded_in_tests)} components excluded from grouping (each needs individual build):"
            )
            # Group by reason to show summary
            direct_bus = [
                c
                for c in excluded_in_tests
                if "NEEDS MIGRATION" in non_groupable_reasons.get(c, "")
            ]
            if direct_bus:
                print(
                    f"\n  ⚠⚠⚠ {len(direct_bus)} DEFINE BUSES DIRECTLY - NEED MIGRATION TO PACKAGES:"
                )
                for comp in sorted(direct_bus):
                    print(f"    - {comp}")

            other_reasons = [
                c
                for c in excluded_in_tests
                if "NEEDS MIGRATION" not in non_groupable_reasons.get(c, "")
            ]
            if other_reasons and len(other_reasons) <= 10:
                print("\n  Other non-groupable components:")
                for comp in sorted(other_reasons):
                    reason = non_groupable_reasons[comp]
                    print(f"    - {comp}: {reason}")
            elif other_reasons:
                print(
                    f"\n  Other non-groupable components: {len(other_reasons)} components"
                )

    # Distribute no_buses components into other groups to maximize efficiency
    # Components with no buses can merge with any bus group since they have no conflicting requirements
    no_buses_by_platform: dict[str, list[str]] = {}
    for (platform, signature), components in list(grouped_components.items()):
        if signature == NO_BUSES_SIGNATURE:
            no_buses_by_platform[platform] = components
            # Remove from grouped_components - we'll distribute them
            del grouped_components[(platform, signature)]

    # Distribute no_buses components into existing groups for each platform
    for platform, no_buses_comps in no_buses_by_platform.items():
        # Find all non-empty groups for this platform (excluding no_buses)
        platform_groups = [
            (sig, comps)
            for (plat, sig), comps in grouped_components.items()
            if plat == platform and sig != NO_BUSES_SIGNATURE
        ]

        if platform_groups:
            # Distribute no_buses components round-robin across existing groups
            for i, comp in enumerate(no_buses_comps):
                sig, _ = platform_groups[i % len(platform_groups)]
                grouped_components[(platform, sig)].append(comp)
        else:
            # No other groups for this platform - keep no_buses components together
            grouped_components[(platform, NO_BUSES_SIGNATURE)] = no_buses_comps

    groups_to_test = []
    individual_tests = set()  # Use set to avoid duplicates

    for (platform, signature), components in sorted(grouped_components.items()):
        if len(components) > 1:
            groups_to_test.append((platform, signature, components))
        # Note: Don't add single-component groups to individual_tests here
        # They'll be added below when we check for ungrouped components

    # Add components that weren't grouped on any platform
    for component in all_tests:
        if component not in [c for _, _, comps in groups_to_test for c in comps]:
            individual_tests.add(component)

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
        sorted_individual = sorted(individual_tests)
        for comp in sorted_individual[:10]:
            print(f"  - {comp}")
        if len(individual_tests) > 10:
            print(f"  ... and {len(individual_tests) - 10} more")

    # Calculate actual build counts based on test files, not component counts
    # Without grouping: every test file would be built separately
    total_test_files = sum(len(test_files) for test_files in all_tests.values())

    # With grouping:
    # - 1 build per group (regardless of how many components)
    # - Individual components still need all their platform builds
    individual_test_file_count = sum(
        len(all_tests[comp]) for comp in individual_tests if comp in all_tests
    )

    total_grouped_components = sum(len(comps) for _, _, comps in groups_to_test)
    total_builds_with_grouping = len(groups_to_test) + individual_test_file_count
    builds_saved = total_test_files - total_builds_with_grouping

    print(f"\n{'=' * 80}")
    print(
        f"Summary: {total_builds_with_grouping} builds total (vs {total_test_files} without grouping)"
    )
    print(
        f"  • {len(groups_to_test)} grouped builds ({total_grouped_components} components)"
    )
    print(
        f"  • {individual_test_file_count} individual builds ({len(individual_tests)} components)"
    )
    if total_test_files > 0:
        reduction_pct = (builds_saved / total_test_files) * 100
        print(f"  • Saves {builds_saved} builds ({reduction_pct:.1f}% reduction)")
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
            test_result = run_grouped_test(
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

            # Store test result
            test_results.append(test_result)

    return tested_components, test_results


def run_individual_component_test(
    component: str,
    test_file: Path,
    platform: str,
    platform_with_version: str,
    base_file: Path,
    build_dir: Path,
    esphome_command: str,
    continue_on_fail: bool,
    tested_components: set[tuple[str, str]],
    test_results: list[TestResult],
) -> None:
    """Run an individual component test if not already tested in a group.

    Args:
        component: Component name
        test_file: Test file path
        platform: Platform name
        platform_with_version: Platform with version
        base_file: Base file for platform
        build_dir: Build directory
        esphome_command: ESPHome command
        continue_on_fail: Whether to continue on failure
        tested_components: Set of already tested components
        test_results: List to append test results
    """
    # Skip if already tested in a group
    if (component, platform_with_version) in tested_components:
        return

    test_result = run_esphome_test(
        component=component,
        test_file=test_file,
        platform=platform,
        platform_with_version=platform_with_version,
        base_file=base_file,
        build_dir=build_dir,
        esphome_command=esphome_command,
        continue_on_fail=continue_on_fail,
    )
    test_results.append(test_result)


def test_components(
    component_patterns: list[str],
    platform_filter: str | None,
    esphome_command: str,
    continue_on_fail: bool,
    enable_grouping: bool = True,
    isolated_components: set[str] | None = None,
) -> int:
    """Test components with optional intelligent grouping.

    Args:
        component_patterns: List of component name patterns
        platform_filter: Optional platform to filter by
        esphome_command: ESPHome command (config/compile)
        continue_on_fail: Whether to continue on failure
        enable_grouping: Whether to enable component grouping
        isolated_components: Set of component names to test in isolation (not grouped).
            These are tested WITHOUT --testing-mode to enable full validation
            (pin conflicts, etc). This is used in CI for directly changed components
            to catch issues that would be missed with --testing-mode.

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
    test_results = []
    tested_components = set()  # Track which components were tested in groups

    # First, run grouped tests if grouping is enabled
    if enable_grouping:
        tested_components, grouped_results = run_grouped_component_tests(
            all_tests=all_tests,
            platform_filter=platform_filter,
            platform_bases=platform_bases,
            tests_dir=tests_dir,
            build_dir=build_dir,
            esphome_command=esphome_command,
            continue_on_fail=continue_on_fail,
            additional_isolated=isolated_components,
        )
        test_results.extend(grouped_results)

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
                        run_individual_component_test(
                            component=component,
                            test_file=test_file,
                            platform=plat,
                            platform_with_version=platform_with_version,
                            base_file=base_file,
                            build_dir=build_dir,
                            esphome_command=esphome_command,
                            continue_on_fail=continue_on_fail,
                            tested_components=tested_components,
                            test_results=test_results,
                        )
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

                    run_individual_component_test(
                        component=component,
                        test_file=test_file,
                        platform=platform,
                        platform_with_version=platform_with_version,
                        base_file=base_file,
                        build_dir=build_dir,
                        esphome_command=esphome_command,
                        continue_on_fail=continue_on_fail,
                        tested_components=tested_components,
                        test_results=test_results,
                    )

    # Separate results into passed and failed
    passed_results = [r for r in test_results if r.success]
    failed_results = [r for r in test_results if not r.success]

    # Print summary
    print("\n" + "=" * 80)
    print(f"Test Summary: {len(passed_results)} passed, {len(failed_results)} failed")
    print("=" * 80)

    if failed_results:
        print("\nFailed tests:")
        for result in failed_results:
            print(f"  - {result.test_id}")

        # Print simplified commands grouped by platform and test type for easy copy-paste
        print("\n" + "=" * 80)
        print("Commands to reproduce failures (copy-paste to reproduce locally):")
        print("=" * 80)
        platform_components = group_components_by_platform(failed_results)
        for platform, test_type in sorted(platform_components.keys()):
            components_csv = ",".join(platform_components[(platform, test_type)])
            print(
                f"script/test_build_components.py -c {components_csv} -t {platform} -e {test_type}"
            )
        print()

    # Write GitHub Actions job summary if in CI
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        write_github_summary(test_results)

    if failed_results:
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
    parser.add_argument(
        "--isolate",
        help="Comma-separated list of components to test in isolation (not grouped with others). "
        "These are tested WITHOUT --testing-mode to enable full validation. "
        "Used in CI for directly changed components to catch pin conflicts and other issues.",
    )

    args = parser.parse_args()

    # Parse component patterns
    component_patterns = [p.strip() for p in args.components.split(",")]

    # Parse isolated components
    isolated_components = None
    if args.isolate:
        isolated_components = {c.strip() for c in args.isolate.split(",") if c.strip()}

    return test_components(
        component_patterns=component_patterns,
        platform_filter=args.target,
        esphome_command=args.esphome_command,
        continue_on_fail=args.continue_on_fail,
        enable_grouping=not args.no_grouping,
        isolated_components=isolated_components,
    )


if __name__ == "__main__":
    sys.exit(main())
