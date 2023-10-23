import json
import os

from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.helpers import read_file


class Extend:
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return f"!extend {self.value}"

    def __eq__(self, b):
        """
        Check if two Extend objects contain the same ID.

        Only used in unit tests.
        """
        return isinstance(b, Extend) and self.value == b.value


def read_config_file(path: str) -> str:
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
        if isinstance(new, dict):
            if not isinstance(old, dict):
                return new
            res = old.copy()
            for k, v in new.items():
                res[k] = merge(old[k], v) if k in old else v
            return res
        if isinstance(new, list):
            if not isinstance(old, list):
                return new
            res = old.copy()
            ids = {
                v[CONF_ID]: i
                for i, v in enumerate(res)
                if CONF_ID in v and isinstance(v[CONF_ID], str)
            }
            for v in new:
                if CONF_ID in v:
                    new_id = v[CONF_ID]
                    if isinstance(new_id, Extend):
                        new_id = new_id.value
                        if new_id in ids:
                            v[CONF_ID] = new_id
                            res[ids[new_id]] = merge(res[ids[new_id]], v)
                            continue
                    else:
                        ids[new_id] = len(res)
                res.append(v)
            return res
        if new is None:
            return old

        return new

    return merge(full_old, full_new)
