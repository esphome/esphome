#!/usr/bin/env python3

import json

from helpers import get_all_component_files, get_components_with_dependencies

from esphome.automation import ACTION_REGISTRY, CONDITION_REGISTRY
from esphome.pins import PIN_SCHEMA_REGISTRY

if __name__ == "__main__":
    files = get_all_component_files()
    components = get_components_with_dependencies(files, True)

    dump = {
        "actions": sorted(list(ACTION_REGISTRY.keys())),
        "conditions": sorted(list(CONDITION_REGISTRY.keys())),
        "pin_providers": sorted(list(PIN_SCHEMA_REGISTRY.keys())),
    }

    print(json.dumps(dump, indent=2))
