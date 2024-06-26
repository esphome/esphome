from __future__ import annotations
import json
import os
from io import StringIO
from typing import Any

from esphome.yaml_util import parse_yaml
from esphome.config import validate_config, _format_vol_invalid, Config
from esphome.core import CORE, DocumentRange
import esphome.config_validation as cv


def _get_invalid_range(res: Config, invalid: cv.Invalid) -> DocumentRange | None:
    return res.get_deepest_document_range_for_path(
        invalid.path, invalid.error_message == "extra keys not allowed"
    )


def _dump_range(range: DocumentRange | None) -> dict | None:
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


def _read_file_content_from_json_on_stdin() -> str:
    """Read the content of a json encoded file from stdin."""
    data = json.loads(input())
    assert data["type"] == "file_response"
    return data["content"]


def _print_file_read_event(path: str) -> None:
    """Print a file read event."""
    print(
        json.dumps(
            {
                "type": "read_file",
                "path": path,
            }
        )
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

        file_name = CORE.config_path
        _print_file_read_event(file_name)
        raw_yaml = _read_file_content_from_json_on_stdin()
        command_line_substitutions: dict[str, Any] = (
            dict(args.substitution) if args.substitution else {}
        )
        vs = VSCodeResult()
        try:
            config = parse_yaml(file_name, StringIO(raw_yaml))
            res = validate_config(config, command_line_substitutions)
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
