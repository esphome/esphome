#!/usr/bin/env python3
from pathlib import Path
import sys
import argparse

from helpers import git_ls_files, changed_files
from esphome.loader import get_component, get_platform
from esphome.core import CORE
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)


def filter_component_files(str):
    return str.startswith("esphome/components/") | str.startswith("tests/components/")


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


def create_components_graph():
    # The root directory of the repo
    root = Path(__file__).parent.parent
    components_dir = root / "esphome" / "components"
    # Fake some directory so that get_component works
    CORE.config_path = str(root)
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

        for dependency in comp.dependencies:
            add_item_to_components_graph(
                components_graph, dependency.split(".")[0], name
            )

        for target_config in TARGET_CONFIGURATIONS:
            CORE.data[KEY_CORE] = target_config
            for auto_load in comp.auto_load:
                add_item_to_components_graph(components_graph, auto_load, name)
        # restore config
        CORE.data[KEY_CORE] = TARGET_CONFIGURATIONS[0]

        for platform_path in path.iterdir():
            platform_name = platform_path.stem
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
                for auto_load in platform.auto_load:
                    add_item_to_components_graph(components_graph, auto_load, name)
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

    files = git_ls_files()
    files = filter(filter_component_files, files)

    if args.changed:
        if args.branch:
            changed = changed_files(args.branch)
        else:
            changed = changed_files()
        # If any base test file(s) changed, there's no need to filter out components
        if not any("tests/test_build_components" in file for file in changed):
            files = [f for f in files if f in changed]

    for c in get_components(files, args.changed):
        print(c)


if __name__ == "__main__":
    main()
