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


def merge_config(full_old, full_new):
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
            if not isinstance(old, list):
                return new
            res = old.copy()
            ids = {v[CONF_ID]: i for i, v in enumerate(res) if CONF_ID in v}
            for v in new:
                if CONF_ID in v and v[CONF_ID] in ids:
                    res[ids[v[CONF_ID]]] = merge(res[ids[v[CONF_ID]]], v)
                    # Delete from the dict so that duplicate components still
                    # get passed through to validation.
                    del ids[v[CONF_ID]]
                    continue
                res.append(v)
            return res
        elif new is None:
            return old

        return new

    return merge(full_old, full_new)
