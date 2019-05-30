from __future__ import print_function

import codecs
import json
import os

from esphome.core import CORE, EsphomeError
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

    try:
        with codecs.open(path, encoding='utf-8') as handle:
            return handle.read()
    except IOError as exc:
        raise EsphomeError(u"Error accessing file {}: {}".format(path, exc))
    except UnicodeDecodeError as exc:
        raise EsphomeError(u"Unable to read file {}: {}".format(path, exc))
