#!/usr/bin/env python3
import argparse
from collections.abc import Callable
from pathlib import Path
import sys

from helpers import changed_files, git_ls_files

from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE
from esphome.loader import ComponentManifest, get_component, get_platform


def filter_component_files(str):
    return str.startswith("esphome/components/") | str.startswith("tests/components/")


def get_all_component_files() -> list[str]:
    """Get all component files from git."""
    files = git_ls_files()
    return list(filter(filter_component_files, files))


def extract_component_names_array_from_files_array(files):
    components = []
    for file in files:
        file_parts = file.split("/")
        if len(file_parts) >= 4:
            component_name = file_parts[2]
            if component_name not in components:
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


def create_components_graph():
    # The root directory of the repo
    root = Path(__file__).parent.parent
    components_dir = root / "esphome" / "components"
    # Fake some directory so that get_component works
    CORE.config_path = root
    # Various configuration to capture different outcomes used by `AUTO_LOAD` function.
    TARGET_CONFIGURATIONS = [
        {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: None},
        {KEY_TARGET_FRAMEWORK: "arduino", KEY_TARGET_PLATFORM: None},
        {KEY_TARGET_FRAMEWORK: "esp-idf", KEY_TARGET_PLATFORM: None},
        {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: PLATFORM_ESP32},
        {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: PLATFORM_ESP8266},
    ]
    CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

    components_graph = {}
    platforms = []
    components: list[tuple[ComponentManifest, str, Path]] = []

    for path in components_dir.iterdir():
        if not path.is_dir():
            continue
        if not (path / "__init__.py").is_file():
            continue
        name = path.name
        comp = get_component(name)
        if comp is None:
            print(
                f"Cannot find component {name}. Make sure current path is pip installed ESPHome"
            )
            sys.exit(1)

        components.append((comp, name, path))
        if comp.is_platform_component:
            platforms.append(name)

    platforms = set(platforms)

    for comp, name, path in components:
        for dependency in comp.dependencies:
            add_item_to_components_graph(
                components_graph, dependency.split(".")[0], name
            )

        for target_config in TARGET_CONFIGURATIONS:
            CORE.data[KEY_CORE] = target_config
            for item in resolve_auto_load(comp.auto_load, config=None):
                add_item_to_components_graph(components_graph, item, name)
        # restore config
        CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

        for platform_path in path.iterdir():
            platform_name = platform_path.stem
            if platform_name == name or platform_name not in platforms:
                continue
            platform = get_platform(platform_name, name)
            if platform is None:
                continue

            add_item_to_components_graph(components_graph, platform_name, name)

            for dependency in platform.dependencies:
                add_item_to_components_graph(
                    components_graph, dependency.split(".")[0], name
                )

            for target_config in TARGET_CONFIGURATIONS:
                CORE.data[KEY_CORE] = target_config
                for item in resolve_auto_load(platform.auto_load, config={}):
                    add_item_to_components_graph(components_graph, item, name)
            # restore config
            CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

    return components_graph


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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-c",
        "--changed",
        action="store_true",
        help="List all components required for testing based on changes",
    )
    parser.add_argument(
        "-b", "--branch", help="Branch to compare changed files against"
    )
    args = parser.parse_args()

    if args.branch and not args.changed:
        parser.error("--branch requires --changed")

    if args.changed:
        # When --changed is passed, only get the changed files
        changed = changed_files(args.branch)

        # If any base test file(s) changed, there's no need to filter out components
        if any("tests/test_build_components" in file for file in changed):
            # Need to get all component files
            files = get_all_component_files()
        else:
            # Only look at changed component files
            files = [f for f in changed if filter_component_files(f)]
    else:
        # Get all component files
        files = get_all_component_files()

    for c in get_components(files, args.changed):
        print(c)


if __name__ == "__main__":
    main()
