#!/usr/bin/env python3
from pathlib import Path
import sys
import argparse

from helpers import git_ls_files, changed_files
from esphome.loader import get_component, get_platform
from esphome.core import CORE
from esphome.const import KEY_CORE, KEY_TARGET_FRAMEWORK, KEY_TARGET_PLATFORM


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


def create_components_graph():
    # The root directory of the repo
    root = Path(__file__).parent.parent
    components_dir = root / "esphome" / "components"
    # Fake some directory so that get_component works
    CORE.config_path = str(root)
    CORE.data[KEY_CORE] = {KEY_TARGET_FRAMEWORK: None, KEY_TARGET_PLATFORM: None}

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
            if dependency != name:
                if dependency not in components_graph:
                    components_graph[dependency] = []
                if name not in components_graph[dependency]:
                    components_graph[dependency].append(name)

        for auto_load in comp.auto_load:
            if auto_load != name:
                if auto_load not in components_graph:
                    components_graph[auto_load] = []
                if name not in components_graph[auto_load]:
                    components_graph[auto_load].append(name)

        for platform_path in path.iterdir():
            platform_name = platform_path.stem
            platform = get_platform(platform_name, name)
            if platform is None:
                continue

            if platform_name != name:
                if platform_name not in components_graph:
                    components_graph[platform_name] = []
                if name not in components_graph[platform_name]:
                    components_graph[platform_name].append(name)

            for dependency in platform.dependencies:
                if dependency != name:
                    if dependency not in components_graph:
                        components_graph[dependency] = []
                    if name not in components_graph[dependency]:
                        components_graph[dependency].append(name)

            for auto_load in platform.auto_load:
                if auto_load != name:
                    if auto_load not in components_graph:
                        components_graph[auto_load] = []
                    if name not in components_graph[auto_load]:
                        components_graph[auto_load].append(name)

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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-c", "--changed", action="store_true", help="Only run on changed files"
    )
    args = parser.parse_args()

    files = git_ls_files()
    files = filter(filter_component_files, files)

    if args.changed:
        changed = changed_files()
        files = [f for f in files if f in changed]

    components = extract_component_names_array_from_files_array(files)

    if args.changed:
        components_graph = create_components_graph()

        all_changed_components = components.copy()
        for c in components:
            all_changed_components.extend(
                find_children_of_component(components_graph, c)
            )
        # Remove duplicate values
        all_changed_components = list(set(all_changed_components))

        for c in sorted(all_changed_components):
            print(c)
    else:
        for c in sorted(components):
            print(c)


if __name__ == "__main__":
    main()
