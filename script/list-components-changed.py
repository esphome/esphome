#!/usr/bin/env python3

from helpers import git_ls_files, changed_files


def filter_component_files(str):
    return str.startswith("esphome/components/")


def extract_component_names_array_from_files_array(files):
    components = []
    for file in files:
        file_parts = file.split("/")
        if len(file_parts) >= 3:
            component_name = file_parts[2]
            if component_name not in components:
                components.append(component_name)
    return components


def main():
    files = git_ls_files()
    files = filter(filter_component_files, files)
    changed = changed_files()
    files = [f for f in files if f in changed]
    files.sort()
    components = extract_component_names_array_from_files_array(files)
    for c in components:
        print(c)


if __name__ == "__main__":
    main()
