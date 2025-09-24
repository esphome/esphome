from __future__ import annotations

from io import StringIO
import json
import os
from typing import Any

from esphome.config import Config, _format_vol_invalid, validate_config
import esphome.config_validation as cv
from esphome.const import __version__ as ESPHOME_VERSION
from esphome.core import CORE, DocumentRange
from esphome.yaml_util import parse_yaml


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


def _request_and_get_stream_on_stdin(fname: str) -> StringIO:
    _print_file_read_event(fname)
    raw_yaml_stream = StringIO(_read_file_content_from_json_on_stdin())
    return raw_yaml_stream


def _vscode_loader(fname: str) -> dict[str, Any]:
    raw_yaml_stream = _request_and_get_stream_on_stdin(fname)
    # it is required to set the name on StringIO so document on start_mark
    # is set properly. Otherwise it is initialized with "<file>"
    raw_yaml_stream.name = fname
    return parse_yaml(fname, raw_yaml_stream, _vscode_loader)


def _ace_loader(fname: str) -> dict[str, Any]:
    raw_yaml_stream = _request_and_get_stream_on_stdin(fname)
    return parse_yaml(fname, raw_yaml_stream)


def _print_version():
    """Print ESPHome version."""
    print(
        json.dumps(
            {
                "type": "version",
                "value": ESPHOME_VERSION,
            }
        )
    )


def read_config(args):
    _print_version()

    while True:
        CORE.reset()
        data = json.loads(input())
        assert data["type"] == "validate" or data["type"] == "exit"
        if data["type"] == "exit":
            return
        CORE.vscode = True
        if args.ace:  # Running from ESPHome Compiler dashboard, not vscode
            CORE.config_path = os.path.join(args.configuration, data["file"])
            loader = _ace_loader
        else:
            CORE.config_path = data["file"]
            loader = _vscode_loader

        file_name = CORE.config_path
        command_line_substitutions: dict[str, Any] = (
            dict(args.substitution) if args.substitution else {}
        )
        vs = VSCodeResult()
        try:
            config = loader(file_name)
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
