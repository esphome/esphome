#!/usr/bin/env python3
"""Determine which CI jobs should run based on changed files.

This script is a centralized way to determine which CI jobs need to run based on
what files have changed. It outputs JSON with the following structure:

{
  "integration_tests": true/false,
  "clang_tidy": true/false,
  "clang_format": true/false,
  "python_linters": true/false,
  "changed_components": ["component1", "component2", ...],
  "component_test_count": 5,
  "memory_impact": {
    "should_run": "true/false",
    "components": ["component1", "component2", ...],
    "platform": "esp32-idf",
    "use_merged_config": "true"
  }
}

The CI workflow uses this information to:
- Skip or run integration tests
- Skip or run clang-tidy (and whether to do a full scan)
- Skip or run clang-format
- Skip or run Python linters (ruff, flake8, pylint, pyupgrade)
- Determine which components to test individually
- Decide how to split component tests (if there are many)
- Run memory impact analysis whenever there are changed components (merged config), and also for core-only changes

Usage:
  python script/determine-jobs.py [-b BRANCH]

Options:
  -b, --branch BRANCH  Branch to compare against (default: dev)
"""

from __future__ import annotations

import argparse
from collections import Counter
from enum import StrEnum
from functools import cache
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

from helpers import (
    BASE_BUS_COMPONENTS,
    CPP_FILE_EXTENSIONS,
    PYTHON_FILE_EXTENSIONS,
    changed_files,
    get_all_dependencies,
    get_component_from_path,
    get_component_test_files,
    get_components_from_integration_fixtures,
    parse_test_filename,
    root_path,
)


class Platform(StrEnum):
    """Platform identifiers for memory impact analysis."""

    ESP8266_ARD = "esp8266-ard"
    ESP32_IDF = "esp32-idf"
    ESP32_C3_IDF = "esp32-c3-idf"
    ESP32_C6_IDF = "esp32-c6-idf"
    ESP32_S2_IDF = "esp32-s2-idf"
    ESP32_S3_IDF = "esp32-s3-idf"


# Memory impact analysis constants
MEMORY_IMPACT_FALLBACK_COMPONENT = "api"  # Representative component for core changes
MEMORY_IMPACT_FALLBACK_PLATFORM = Platform.ESP32_IDF  # Most representative platform

# Platform preference order for memory impact analysis
# Prefer newer platforms first as they represent the future of ESPHome
# ESP8266 is most constrained but many new features don't support it
MEMORY_IMPACT_PLATFORM_PREFERENCE = [
    Platform.ESP32_C6_IDF,  # ESP32-C6 IDF (newest, supports Thread/Zigbee)
    Platform.ESP8266_ARD,  # ESP8266 Arduino (most memory constrained - best for impact analysis)
    Platform.ESP32_IDF,  # ESP32 IDF platform (primary ESP32 platform, most representative)
    Platform.ESP32_C3_IDF,  # ESP32-C3 IDF
    Platform.ESP32_S2_IDF,  # ESP32-S2 IDF
    Platform.ESP32_S3_IDF,  # ESP32-S3 IDF
]


def should_run_integration_tests(branch: str | None = None) -> bool:
    """Determine if integration tests should run based on changed files.

    This function is used by the CI workflow to intelligently skip integration tests when they're
    not needed, saving significant CI time and resources.

    Integration tests will run when ANY of the following conditions are met:

    1. Core C++ files changed (esphome/core/*)
       - Any .cpp, .h, .tcc files in the core directory
       - These files contain fundamental functionality used throughout ESPHome
       - Examples: esphome/core/component.cpp, esphome/core/application.h

    2. Core Python files changed (esphome/core/*.py)
       - Only .py files in the esphome/core/ directory
       - These are core Python files that affect the entire system
       - Examples: esphome/core/config.py, esphome/core/__init__.py
       - NOT included: esphome/*.py, esphome/dashboard/*.py, esphome/components/*/*.py

    3. Integration test files changed
       - Any file in tests/integration/ directory
       - This includes test files themselves and fixture YAML files
       - Examples: tests/integration/test_api.py, tests/integration/fixtures/api.yaml

    4. Components used by integration tests (or their dependencies) changed
       - The function parses all YAML files in tests/integration/fixtures/
       - Extracts which components are used in integration tests
       - Recursively finds all dependencies of those components
       - If any of these components have changes, tests must run
       - Example: If api.yaml uses 'sensor' and 'api' components, and 'api' depends on 'socket',
         then changes to sensor/, api/, or socket/ components trigger tests

    Args:
        branch: Branch to compare against. If None, uses default.

    Returns:
        True if integration tests should run, False otherwise.
    """
    files = changed_files(branch)

    # Check if any core files changed (esphome/core/*)
    for file in files:
        if file.startswith("esphome/core/"):
            return True

    # Check if any integration test files changed
    if any("tests/integration" in file for file in files):
        return True

    # Get all components used in integration tests and their dependencies
    fixture_components = get_components_from_integration_fixtures()
    all_required_components = get_all_dependencies(fixture_components)

    # Check if any required components changed
    for file in files:
        component = get_component_from_path(file)
        if component and component in all_required_components:
            return True

    return False


def should_run_clang_tidy(branch: str | None = None) -> bool:
    """Determine if clang-tidy should run based on changed files.

    This function is used by the CI workflow to intelligently skip clang-tidy checks when they're
    not needed, saving significant CI time and resources.

    Clang-tidy will run when ANY of the following conditions are met:

    1. Clang-tidy configuration changed
       - The hash of .clang-tidy configuration file has changed
       - The hash includes the .clang-tidy file, clang-tidy version from requirements_dev.txt,
         and relevant platformio.ini sections
       - When configuration changes, a full scan is needed to ensure all code complies
         with the new rules
       - Detected by script/clang_tidy_hash.py --check returning exit code 0

    2. Any C++ source files changed
       - Any file with C++ extensions: .cpp, .h, .hpp, .cc, .cxx, .c, .tcc
       - Includes files anywhere in the repository, not just in esphome/
       - This ensures all C++ code is checked, including tests, examples, etc.
       - Examples: esphome/core/component.cpp, tests/custom/my_component.h

    3. The .clang-tidy.hash file itself changed
       - This indicates the configuration has been updated and clang-tidy should run
       - Ensures that PRs updating the clang-tidy configuration are properly validated

    If the hash check fails for any reason, clang-tidy runs as a safety measure to ensure
    code quality is maintained.

    Args:
        branch: Branch to compare against. If None, uses default.

    Returns:
        True if clang-tidy should run, False otherwise.
    """
    # First check if clang-tidy configuration changed (full scan needed)
    try:
        result = subprocess.run(
            [os.path.join(root_path, "script", "clang_tidy_hash.py"), "--check"],
            capture_output=True,
            check=False,
        )
        # Exit 0 means hash changed (full scan needed)
        if result.returncode == 0:
            return True
    except Exception:
        # If hash check fails, run clang-tidy to be safe
        return True

    # Check if .clang-tidy.hash file itself was changed
    # This handles the case where the hash was properly updated in the PR
    files = changed_files(branch)
    if ".clang-tidy.hash" in files:
        return True

    return _any_changed_file_endswith(branch, CPP_FILE_EXTENSIONS)


def should_run_clang_format(branch: str | None = None) -> bool:
    """Determine if clang-format should run based on changed files.

    This function is used by the CI workflow to skip clang-format checks when no C++ files
    have changed, saving CI time and resources.

    Clang-format will run when any C++ source files have changed.

    Args:
        branch: Branch to compare against. If None, uses default.

    Returns:
        True if clang-format should run, False otherwise.
    """
    return _any_changed_file_endswith(branch, CPP_FILE_EXTENSIONS)


def should_run_python_linters(branch: str | None = None) -> bool:
    """Determine if Python linters (ruff, flake8, pylint, pyupgrade) should run based on changed files.

    This function is used by the CI workflow to skip Python linting checks when no Python files
    have changed, saving CI time and resources.

    Python linters will run when any Python source files have changed.

    Args:
        branch: Branch to compare against. If None, uses default.

    Returns:
        True if Python linters should run, False otherwise.
    """
    return _any_changed_file_endswith(branch, PYTHON_FILE_EXTENSIONS)


def _any_changed_file_endswith(branch: str | None, extensions: tuple[str, ...]) -> bool:
    """Check if a changed file ends with any of the specified extensions."""
    return any(file.endswith(extensions) for file in changed_files(branch))


@cache
def _component_has_tests(component: str) -> bool:
    """Check if a component has test files.

    Cached to avoid repeated filesystem operations for the same component.

    Args:
        component: Component name to check

    Returns:
        True if the component has test YAML files
    """
    return bool(get_component_test_files(component))


def detect_memory_impact_config(
    branch: str | None = None,
) -> dict[str, Any]:
    """Determine memory impact analysis configuration.

    Always runs memory impact analysis when there are changed components,
    building a merged configuration with all changed components (like
    test_build_components.py does) to get comprehensive memory analysis.

    Args:
        branch: Branch to compare against

    Returns:
        Dictionary with memory impact analysis parameters:
        - should_run: "true" or "false"
        - components: list of component names to analyze
        - platform: platform name for the merged build
        - use_merged_config: "true" (always use merged config)
    """

    # Get actually changed files (not dependencies)
    files = changed_files(branch)

    # Find all changed components (excluding core and base bus components)
    changed_component_set: set[str] = set()
    has_core_changes = False

    for file in files:
        component = get_component_from_path(file)
        if component:
            # Skip base bus components as they're used across many builds
            if component not in BASE_BUS_COMPONENTS:
                changed_component_set.add(component)
        elif file.startswith("esphome/"):
            # Core ESPHome files changed (not component-specific)
            has_core_changes = True

    # If no components changed but core changed, test representative component
    force_fallback_platform = False
    if not changed_component_set and has_core_changes:
        print(
            f"Memory impact: No components changed, but core files changed. "
            f"Testing {MEMORY_IMPACT_FALLBACK_COMPONENT} component on {MEMORY_IMPACT_FALLBACK_PLATFORM}.",
            file=sys.stderr,
        )
        changed_component_set.add(MEMORY_IMPACT_FALLBACK_COMPONENT)
        force_fallback_platform = True  # Use fallback platform (most representative)
    elif not changed_component_set:
        # No components and no core changes
        return {"should_run": "false"}

    # Find components that have tests and collect their supported platforms
    components_with_tests: list[str] = []
    component_platforms_map: dict[
        str, set[Platform]
    ] = {}  # Track which platforms each component supports

    for component in sorted(changed_component_set):
        # Look for test files on preferred platforms
        test_files = get_component_test_files(component)
        if not test_files:
            continue

        # Check if component has tests for any preferred platform
        available_platforms = [
            platform
            for test_file in test_files
            if (platform := parse_test_filename(test_file)[1]) != "all"
            and platform in MEMORY_IMPACT_PLATFORM_PREFERENCE
        ]

        if not available_platforms:
            continue

        component_platforms_map[component] = set(available_platforms)
        components_with_tests.append(component)

    # If no components have tests, don't run memory impact
    if not components_with_tests:
        return {"should_run": "false"}

    # Find common platforms supported by ALL components
    # This ensures we can build all components together in a merged config
    common_platforms = set(MEMORY_IMPACT_PLATFORM_PREFERENCE)
    for component, platforms in component_platforms_map.items():
        common_platforms &= platforms

    # Select the most preferred platform from the common set
    # Exception: for core changes, use fallback platform (most representative of codebase)
    if force_fallback_platform:
        platform = MEMORY_IMPACT_FALLBACK_PLATFORM
    elif common_platforms:
        # Pick the most preferred platform that all components support
        platform = min(common_platforms, key=MEMORY_IMPACT_PLATFORM_PREFERENCE.index)
    else:
        # No common platform - pick the most commonly supported platform
        # This allows testing components individually even if they can't be merged
        # Count how many components support each platform
        platform_counts = Counter(
            p for platforms in component_platforms_map.values() for p in platforms
        )
        # Pick the platform supported by most components, preferring earlier in MEMORY_IMPACT_PLATFORM_PREFERENCE
        platform = max(
            platform_counts.keys(),
            key=lambda p: (
                platform_counts[p],
                -MEMORY_IMPACT_PLATFORM_PREFERENCE.index(p),
            ),
        )

    # Debug output
    print("Memory impact analysis:", file=sys.stderr)
    print(f"  Changed components: {sorted(changed_component_set)}", file=sys.stderr)
    print(f"  Components with tests: {components_with_tests}", file=sys.stderr)
    print(
        f"  Component platforms: {dict(sorted(component_platforms_map.items()))}",
        file=sys.stderr,
    )
    print(f"  Common platforms: {sorted(common_platforms)}", file=sys.stderr)
    print(f"  Selected platform: {platform}", file=sys.stderr)

    return {
        "should_run": "true",
        "components": components_with_tests,
        "platform": platform,
        "use_merged_config": "true",
    }


def main() -> None:
    """Main function that determines which CI jobs to run."""
    parser = argparse.ArgumentParser(
        description="Determine which CI jobs should run based on changed files"
    )
    parser.add_argument(
        "-b", "--branch", help="Branch to compare changed files against"
    )
    args = parser.parse_args()

    # Determine what should run
    run_integration = should_run_integration_tests(args.branch)
    run_clang_tidy = should_run_clang_tidy(args.branch)
    run_clang_format = should_run_clang_format(args.branch)
    run_python_linters = should_run_python_linters(args.branch)

    # Get both directly changed and all changed components (with dependencies) in one call
    script_path = Path(__file__).parent / "list-components.py"
    cmd = [sys.executable, str(script_path), "--changed-with-deps"]
    if args.branch:
        cmd.extend(["-b", args.branch])

    result = subprocess.run(cmd, capture_output=True, text=True, check=True)
    component_data = json.loads(result.stdout)
    directly_changed_components = component_data["directly_changed"]
    changed_components = component_data["all_changed"]

    # Filter to only components that have test files
    # Components without tests shouldn't generate CI test jobs
    changed_components_with_tests = [
        component for component in changed_components if _component_has_tests(component)
    ]

    # Get directly changed components with tests (for isolated testing)
    # These will be tested WITHOUT --testing-mode in CI to enable full validation
    # (pin conflicts, etc.) since they contain the actual changes being reviewed
    directly_changed_with_tests = [
        component
        for component in directly_changed_components
        if _component_has_tests(component)
    ]

    # Get dependency-only components (for grouped testing)
    dependency_only_components = [
        component
        for component in changed_components_with_tests
        if component not in directly_changed_components
    ]

    # Detect components for memory impact analysis (merged config)
    memory_impact = detect_memory_impact_config(args.branch)

    # Build output
    output: dict[str, Any] = {
        "integration_tests": run_integration,
        "clang_tidy": run_clang_tidy,
        "clang_format": run_clang_format,
        "python_linters": run_python_linters,
        "changed_components": changed_components,
        "changed_components_with_tests": changed_components_with_tests,
        "directly_changed_components_with_tests": directly_changed_with_tests,
        "dependency_only_components_with_tests": dependency_only_components,
        "component_test_count": len(changed_components_with_tests),
        "directly_changed_count": len(directly_changed_with_tests),
        "dependency_only_count": len(dependency_only_components),
        "memory_impact": memory_impact,
    }

    # Output as JSON
    print(json.dumps(output))


if __name__ == "__main__":
    main()
