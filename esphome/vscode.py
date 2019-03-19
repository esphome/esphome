from __future__ import print_function

import json

from esphome import yaml_util
from esphome.config import load_config, _format_vol_invalid
from esphome.core import CORE, EsphomeError
from esphome.py_compat import safe_input, text_type


class VSCodeResult(object):
    def __init__(self):
        self.yaml_errors = []
        self.validation_errors = []
        self.ids = []

    def dump(self):
        return json.dumps({
            'yaml_errors': self.yaml_errors,
            'validation_errors': self.validation_errors,
            'ids': self.ids,
        })

    def add_yaml_error(self, location, message):
        self.yaml_errors.append({
            'location': location,
            'message': message,
        })

    def add_validation_error(self, location, message):
        self.validation_errors.append({
            'location': location,
            'message': message,
        })

    def add_id(self, location, name, type_):
        self.ids.append({
            'location': location,
            'name': name,
            'type': type_,
        })


def read_config():
    CORE.vscode = True
    vs = VSCodeResult()
    try:
        res = load_config()
    except EsphomeError as err:
        vs.add_yaml_error(None, text_type(err))
        print(vs.dump())
        return
    for err in res.errors:
        vs.add_validation_error(None, _format_vol_invalid(err, res))
    print(vs.dump())
