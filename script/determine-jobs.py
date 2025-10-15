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
  "component_test_count": 5
}

The CI workflow uses this information to:
- Skip or run integration tests
- Skip or run clang-tidy (and whether to do a full scan)
- Skip or run clang-format
- Skip or run Python linters (ruff, flake8, pylint, pyupgrade)
- Determine which components to test individually
- Decide how to split component tests (if there are many)

Usage:
  python script/determine-jobs.py [-b BRANCH]

Options:
  -b, --branch BRANCH  Branch to compare against (default: dev)
"""

from __future__ import annotations

import argparse
from functools import cache
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any

from helpers import (
    CPP_FILE_EXTENSIONS,
    ESPHOME_COMPONENTS_PATH,
    PYTHON_FILE_EXTENSIONS,
    changed_files,
    get_all_dependencies,
    get_components_from_integration_fixtures,
    root_path,
)


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
        if file.startswith(ESPHOME_COMPONENTS_PATH):
            parts = file.split("/")
            if len(parts) >= 3:
                component = parts[2]
                if component in all_required_components:
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
    tests_dir = Path(root_path) / "tests" / "components" / component
    if not tests_dir.exists():
        return False
    return any(tests_dir.glob("test.*.yaml"))


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
    }

    # Output as JSON
    print(json.dumps(output))


if __name__ == "__main__":
    main()
