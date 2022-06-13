import json
import os

from esphome.core import CORE
from esphome.helpers import read_file
from esphome.const import CONF_ID


def read_config_file(path):
    # type: (str) -> str
    if CORE.vscode and (
        not CORE.ace or os.path.abspath(path) == os.path.abspath(CORE.config_path)
    ):
        print(
            json.dumps(
                {
                    "type": "read_file",
                    "path": path,
                }
            )
        )
        data = json.loads(input())
        assert data["type"] == "file_response"
        return data["content"]

    return read_file(path)


def merge_config(full_old, full_new):
    def merge_lists(old, new):
        # Merge list entries if CONF_ID is present and identical in both
        res = []
        to_merge = {}
        for v in old:
            if isinstance(v, dict) and CONF_ID in v:
                to_merge[v[CONF_ID]] = v
            else:
                res.append(v)
        for v in new:
            if isinstance(v, dict) and CONF_ID in v and v[CONF_ID] in to_merge:
                id = v[CONF_ID]
                to_merge[id] = merge(to_merge[id], v)
            else:
                res.append(v)
        return res + list(to_merge.values())

    def merge(old, new):
        # pylint: disable=no-else-return
        if isinstance(new, dict):
            if not isinstance(old, dict):
                return new
            res = old.copy()
            for k, v in new.items():
                res[k] = merge(old[k], v) if k in old else v
            return res
        elif isinstance(new, list):
            if isinstance(old, list):
                return merge_lists(old, new)
            return new

        elif new is None:
            return old

        return new

    return merge(full_old, full_new)
