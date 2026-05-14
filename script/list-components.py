#!/usr/bin/env python3
import argparse

from helpers import (
    changed_files,
    filter_component_and_test_cpp_files,
    filter_component_and_test_files,
    get_all_component_files,
    get_components_with_dependencies,
    get_cpp_changed_components,
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-c",
        "--changed",
        action="store_true",
        help="List all components with dependencies (used by clang-tidy). "
        "When base test infrastructure changes, returns ALL components.",
    )
    parser.add_argument(
        "--changed-direct",
        action="store_true",
        help="List only directly changed components, ignoring infrastructure changes "
        "(used by CI for isolation decisions)",
    )
    parser.add_argument(
        "--changed-with-deps",
        action="store_true",
        help="Output JSON with both directly changed and all changed components "
        "(with dependencies), ignoring infrastructure changes (used by CI for test determination)",
    )
    parser.add_argument(
        "-b", "--branch", help="Branch to compare changed files against"
    )
    parser.add_argument(
        "--cpp-changed",
        action="store_true",
        help="List components with changed C++ files",
    )
    args = parser.parse_args()

    if args.branch and not (
        args.changed
        or args.changed_direct
        or args.changed_with_deps
        or args.cpp_changed
    ):
        parser.error(
            "--branch requires --changed, --changed-direct, --changed-with-deps, or --cpp-changed"
        )

    if (
        args.changed
        or args.changed_direct
        or args.changed_with_deps
        or args.cpp_changed
    ):
        # When --changed* is passed, only get the changed files
        changed = changed_files(args.branch)

        # If any base test file(s) changed, we need to check all components
        # BUT only for --changed (used by clang-tidy for comprehensive checking)
        # NOT for --changed-direct or --changed-with-deps (used by CI for targeted testing)
        #
        # Flag usage:
        # - --changed: Used by clang-tidy (script/helpers.py get_changed_components)
        #   Returns: All components with dependencies when base test files change
        #   Reason: Test infrastructure changes may affect any component
        #
        # - --changed-direct: Used by CI isolation (script/determine-jobs.py)
        #   Returns: Only components with actual code changes (not infrastructure)
        #   Reason: Only directly changed components need isolated testing
        #
        # - --changed-with-deps: Used by CI test determination (script/determine-jobs.py)
        #   Returns: Components with code changes + their dependencies (not infrastructure)
        #   Reason: CI needs to test changed components and their dependents
        #
        # - --cpp-changed: Used by CI to determine if any C++ files changed (script/determine-jobs.py)
        #   Returns: Only components with changed C++ files
        #   Reason: Only components with C++ changes need C++ testing

        base_test_changed = any(
            "tests/test_build_components" in file for file in changed
        )

        if base_test_changed and not args.changed_direct and not args.changed_with_deps:
            # Base test infrastructure changed - load all component files
            # This is for --changed (clang-tidy) which needs comprehensive checking
            files = get_all_component_files()
        else:
            # Only look at changed component files (ignore infrastructure changes)
            # For --changed-direct: only actual component code changes matter (for isolation)
            # For --changed-with-deps: only actual component code changes matter (for testing)
            files = [f for f in changed if filter_component_and_test_files(f)]
    else:
        # Get all component files
        files = get_all_component_files()

    if args.changed_with_deps:
        # Return JSON with both directly changed and all changed components
        import json

        directly_changed = get_components_with_dependencies(files, False)
        all_changed = get_components_with_dependencies(files, True)
        output = {
            "directly_changed": directly_changed,
            "all_changed": all_changed,
        }
        print(json.dumps(output))
    elif args.changed_direct:
        # Return only directly changed components (without dependencies)
        for c in get_components_with_dependencies(files, False):
            print(c)
    elif args.cpp_changed:
        # Only look at changed cpp files
        files = list(filter(filter_component_and_test_cpp_files, changed))
        for c in get_cpp_changed_components(files):
            print(c)
    else:
        # Return all changed components (with dependencies) - default behavior
        for c in get_components_with_dependencies(files, args.changed):
            print(c)


if __name__ == "__main__":
    main()
