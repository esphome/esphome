import json
import os

from esphome.core import CORE
from esphome.helpers import read_file


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
    def find_and_merge(old_list, new_item):
        if isinstance(new_item, dict) and "id" in new_item:
            for n, item in enumerate(old_list):
                if (
                    isinstance(item, dict)
                    and "id" in item
                    and item["id"] == new_item["id"]
                ):
                    old_list[n] = merge(item, new_item)
                    return True
        return False

    def merge_list(old, new):
        res = old.copy()
        for new_item in new:
            if not find_and_merge(res, new_item):
                res.append(new_item)
        return res

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
            return merge_list(old, new)
        if new is None:
            return old

        return new

    return merge(full_old, full_new)
