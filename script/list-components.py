#!/usr/bin/env python3
import argparse
from pathlib import Path

from aioesphomeapi import Callable
from helpers import (
    CPP_FILE_EXTENSIONS,
    ESPHOME_COMPONENTS_PATH,
    ESPHOME_TESTS_COMPONENTS_PATH,
    changed_files,
    create_components_graph,
    filter_component_files,
    filter_cpp_files,
    get_component_from_path,
    get_components_with_dependencies,
    git_ls_files,
)


def get_all_component_files() -> list[str]:
    """Get all component files from git."""
    files = git_ls_files()
    return list(filter(filter_component_files, files))


def extract_component_names_array_from_files_array(files):
    components = []
    for file in files:
        component_name = get_component_from_path(file)
        if component_name and component_name not in components:
            components.append(component_name)
    return components


def add_item_to_components_graph(components_graph, parent, child):
    if not parent.startswith("__") and parent != child:
        if parent not in components_graph:
            components_graph[parent] = []
        if child not in components_graph[parent]:
            components_graph[parent].append(child)


def resolve_auto_load(
    auto_load: list[str] | Callable[[], list[str]] | Callable[[dict | None], list[str]],
    config: dict | None = None,
) -> list[str]:
    """Resolve AUTO_LOAD to a list, handling callables with or without config parameter.

    Args:
        auto_load: The AUTO_LOAD value (list or callable)
        config: Optional config to pass to callable AUTO_LOAD functions

    Returns:
        List of component names to auto-load
    """
    if not callable(auto_load):
        return auto_load

    import inspect

    if inspect.signature(auto_load).parameters:
        return auto_load(config)
    return auto_load()


def find_children_of_component(components_graph, component_name, depth=0):
    if component_name not in components_graph:
        return []

    children = []

    for child in components_graph[component_name]:
        children.append(child)
        if depth < 10:
            children.extend(
                find_children_of_component(components_graph, child, depth + 1)
            )
    # Remove duplicate values
    return list(set(children))


def get_components(files: list[str], get_dependencies: bool = False):
    components = extract_component_names_array_from_files_array(files)

    if get_dependencies:
        components_graph = create_components_graph()

        all_components = components.copy()
        for c in components:
            all_components.extend(find_children_of_component(components_graph, c))
        # Remove duplicate values
        all_changed_components = list(set(all_components))

        return sorted(all_changed_components)

    return sorted(components)


def get_cpp_changed_components(files: list[str]):
    components_graph = create_components_graph()
    affected = set()
    for file in files:
        if not file.endswith(CPP_FILE_EXTENSIONS):
            continue
        if file.startswith(ESPHOME_TESTS_COMPONENTS_PATH):
            parts = file.split("/")
            if len(parts) >= 4:
                component_dir = Path(ESPHOME_TESTS_COMPONENTS_PATH) / parts[2]
                if component_dir.is_dir():
                    affected.add(parts[2])
        elif file.startswith(ESPHOME_COMPONENTS_PATH):
            parts = file.split("/")
            if len(parts) >= 4:
                component = parts[2]
                affected.update(find_children_of_component(components_graph, component))
                affected.add(component)
    return sorted(affected)


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
            files = [f for f in changed if filter_component_files(f)]
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
        files = list(filter(filter_cpp_files, changed))
        for c in get_cpp_changed_components(files):
            print(c)
    else:
        # Return all changed components (with dependencies) - default behavior
        for c in get_components_with_dependencies(files, args.changed):
            print(c)


if __name__ == "__main__":
    main()
