import json
import os

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
            return merge_list(old, new)
        elif new is None:
            return old

        return new

    def merge_list(old, new):
        """Merge lists of dicts using the id field.

        This can usefully merge lists of sensors.
        """
        seen_ids = {}
        out = []
        merged = old + new
        for val in merged:
            if not isinstance(val, dict):
                return merged
            if key := val.get("id"):
                if (index := seen_ids.get(key)) is not None:
                    out[index] = merge(out[index], val)
                else:
                    seen_ids[key] = len(out)
                    out.append(val)
            else:
                out.append(val)
        return out

    return merge(full_old, full_new)
