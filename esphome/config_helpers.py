from __future__ import print_function

import json
import os

from esphome.core import CORE
from esphome.helpers import read_file
from esphome.py_compat import safe_input


def read_config_file(path):
    # type: (basestring) -> unicode
    if CORE.vscode and (not CORE.ace or
                        os.path.abspath(path) == os.path.abspath(CORE.config_path)):
        print(json.dumps({
            'type': 'read_file',
            'path': path,
        }))
        data = json.loads(safe_input())
        assert data['type'] == 'file_response'
        return data['content']

    return read_file(path)
