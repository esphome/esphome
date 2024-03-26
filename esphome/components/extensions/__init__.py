from typing import Any, Union
import esphome.config_validation as cv
from esphome.config_helpers import merge_config
from esphome.const import (
    CONF_EXTENSIONS,
    CONF_ID,
)

CODEOWNERS = ["@kpfleming"]

DOMAIN = CONF_EXTENSIONS

CONFIG_SCHEMA = cv.Schema(
    {
        cv.validate_id_name: dict,
    }
)


class ObjectInDict:
    def __init__(self, dic: dict, key: str):
        self.dic = dic
        self.key = key


class ObjectInList:
    def __init__(self, lis: list, index: int):
        self.lis = lis
        self.index = index


def gather_object_ids(config: dict) -> dict[str, Union[ObjectInDict, ObjectInList]]:
    result = {}
    for k, v in config.items():
        if k == CONF_EXTENSIONS:
            continue
        if isinstance(v, dict):
            if CONF_ID in v and isinstance(v[CONF_ID], str):
                result[v[CONF_ID]] = ObjectInDict(config, k)
            result.update(gather_object_ids(v))
        elif isinstance(v, list):
            for i, o in enumerate(v):
                if isinstance(o, dict):
                    if CONF_ID in o and isinstance(o[CONF_ID], str):
                        result[o[CONF_ID]] = ObjectInList(v, i)
                    result.update(gather_object_ids(o))
    return result


def do_extensions_pass(config: dict) -> dict[str, Any]:
    if CONF_EXTENSIONS not in config:
        return config
    extensions = config[CONF_EXTENSIONS]
    with cv.prepend_path(CONF_EXTENSIONS):
        extensions = CONFIG_SCHEMA(extensions)

        while extensions:
            matched = False
            ids = gather_object_ids(config)

            for id in [id for id in extensions.keys() if id in ids]:
                matched = True
                obj = ids[id]
                if isinstance(obj, ObjectInDict):
                    obj.dic[obj.key] = merge_config(obj.dic[obj.key], extensions[id])
                elif isinstance(obj, ObjectInList):
                    obj.lis[obj.index] = merge_config(
                        obj.lis[obj.index], extensions[id]
                    )
                del extensions[id]

            if not matched:
                break

        # if any extension IDs were not found, report them
        if extensions:
            raise cv.MultipleInvalid(
                [
                    cv.Invalid(f"No match for extension ID {k} found in configuration.")
                    for k in extensions.keys()
                ]
            )

        del config[CONF_EXTENSIONS]
    return config
