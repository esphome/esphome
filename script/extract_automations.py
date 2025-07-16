#!/usr/bin/env python3

import json

from helpers import git_ls_files

from esphome.automation import ACTION_REGISTRY, CONDITION_REGISTRY
from esphome.pins import PIN_SCHEMA_REGISTRY

list_components = __import__("list-components")


if __name__ == "__main__":
    files = git_ls_files()
    files = filter(list_components.filter_component_files, files)

    components = list_components.get_components(files, True)

    dump = {
        "actions": sorted(list(ACTION_REGISTRY.keys())),
        "conditions": sorted(list(CONDITION_REGISTRY.keys())),
        "pin_providers": sorted(list(PIN_SCHEMA_REGISTRY.keys())),
    }

    print(json.dumps(dump, indent=2))
