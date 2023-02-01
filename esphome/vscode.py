import json
import os

from typing import Optional

from esphome.config import load_config, _format_vol_invalid, Config
from esphome.core import CORE, DocumentRange
import esphome.config_validation as cv


def _get_invalid_range(res: Config, invalid: cv.Invalid) -> Optional[DocumentRange]:
    return res.get_deepest_document_range_for_path(
        invalid.path, invalid.error_message == "extra keys not allowed"
    )


def _dump_range(range: Optional[DocumentRange]) -> Optional[dict]:
    if range is None:
        return None
    return {
        "document": range.start_mark.document,
        "start_line": range.start_mark.line,
        "start_col": range.start_mark.column,
        "end_line": range.end_mark.line,
        "end_col": range.end_mark.column,
    }


class VSCodeResult:
    def __init__(self):
        self.yaml_errors = []
        self.validation_errors = []

    def dump(self):
        return json.dumps(
            {
                "type": "result",
                "yaml_errors": self.yaml_errors,
                "validation_errors": self.validation_errors,
            }
        )

    def add_yaml_error(self, message):
        self.yaml_errors.append(
            {
                "message": message,
            }
        )

    def add_validation_error(self, range_, message):
        self.validation_errors.append(
            {
                "range": _dump_range(range_),
                "message": message,
            }
        )


def read_config(args):
    while True:
        CORE.reset()
        data = json.loads(input())
        assert data["type"] == "validate"
        CORE.vscode = True
        CORE.ace = args.ace
        f = data["file"]
        if CORE.ace:
            CORE.config_path = os.path.join(args.configuration, f)
        else:
            CORE.config_path = data["file"]
        vs = VSCodeResult()
        try:
            res = load_config(dict(args.substitution) if args.substitution else {})
        except Exception as err:  # pylint: disable=broad-except
            vs.add_yaml_error(str(err))
        else:
            for err in res.errors:
                try:
                    range_ = _get_invalid_range(res, err)
                    vs.add_validation_error(range_, _format_vol_invalid(err, res))
                except Exception:  # pylint: disable=broad-except
                    continue
        print(vs.dump())
