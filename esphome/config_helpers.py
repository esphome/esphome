import json
import os

from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.helpers import read_file


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


def merge_config(old, new):
    def merge_lists(old, new):
        res = old.copy()
        ids = {v[CONF_ID]: i for i, v in enumerate(res) if CONF_ID in v}
        for v in new:
            if CONF_ID in v and v[CONF_ID] in ids:
                res[ids[v[CONF_ID]]] = merge_config(res[ids[v[CONF_ID]]], v)
                # Delete from the dict so that duplicate components still
                # get passed through to validation.
                del ids[v[CONF_ID]]
                continue
            res.append(v)
        return res

    # pylint: disable=no-else-return
    if isinstance(new, dict):
        if not isinstance(old, dict):
            return new
        res = old.copy()
        for k, v in new.items():
            res[k] = merge_config(old[k], v) if k in old else v
        return res
    elif isinstance(new, list):
        if isinstance(old, list):
            return merge_lists(old, new)
        return new

    elif new is None:
        return old

    return new
