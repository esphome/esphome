"""Tests for the `--error-format line` output of read_config."""

from pathlib import Path

import pytest
import voluptuous as vol
import yaml

from esphome import config_validation as cv
from esphome.config import (
    Config,
    InvalidYAMLError,
    _print_line_errors,
    _print_line_load_error,
    _print_line_message,
    _print_marked_yaml_error,
    read_config,
)
from esphome.const import ErrorFormat
from esphome.core import CORE
from esphome.voluptuous_schema import ExtraKeysInvalid, KeyInvalid


def _read(
    tmp_path: Path, files: dict[str, str], capsys: pytest.CaptureFixture[str]
) -> list[str]:
    for name, content in files.items():
        (tmp_path / name).write_text(content)
    CORE.config_path = tmp_path / "test.yaml"
    CORE.error_format = ErrorFormat.LINE
    assert read_config({}) is None
    return capsys.readouterr().out.splitlines()


def test_validation_error(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    lines = _read(
        tmp_path,
        {"test.yaml": "esphome:\n  name: test\nhost:\nlogger:\n  levl: DEBUG\n"},
        capsys,
    )
    assert lines == [
        (
            f"{tmp_path / 'test.yaml'}:5:3: error: [levl] is an invalid option for "
            "[logger]. Did you mean [level]?"
        )
    ]


def test_validation_error_in_package(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    lines = _read(
        tmp_path,
        {
            "test.yaml": (
                "esphome:\n  name: test\nhost:\npackages:\n  pkg: !include pkg.yaml\n"
            ),
            "pkg.yaml": "logger:\n  levl: DEBUG\n",
        },
        capsys,
    )
    assert lines == [
        (
            f"{tmp_path / 'pkg.yaml'}:2:3: error: [levl] is an invalid option for "
            "[logger]. Did you mean [level]?"
        )
    ]


def test_unknown_action_anchors_on_the_key(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    # validate_registry_entry raises KeyInvalid for an unknown action, so the
    # location is the key, not the action's value.
    lines = _read(
        tmp_path,
        {
            "test.yaml": (
                "esphome:\n  name: test\nhost:\nlogger:\nbutton:\n"
                '  - platform: template\n    on_press:\n      - loggr.log: "msg"\n'
            )
        },
        capsys,
    )
    assert lines == [
        (
            f"{tmp_path / 'test.yaml'}:8:9: error: Unable to find action "
            "with the name 'loggr.log'."
        )
    ]


def test_yaml_syntax_error(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    lines = _read(tmp_path, {"test.yaml": "esphome:\n  name: [1, 2\nhost:\n"}, capsys)
    assert lines == [
        f"{tmp_path / 'test.yaml'}:3:5: error: expected ',' or ']', but got ':'",
        f"{tmp_path / 'test.yaml'}:2:9: note: while parsing a flow sequence",
    ]


def test_yaml_syntax_error_in_package(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    lines = _read(
        tmp_path,
        {
            "test.yaml": (
                "esphome:\n  name: test\nhost:\npackages:\n  pkg: !include pkg.yaml\n"
            ),
            "pkg.yaml": "logger:\n  level: [1, 2\nwifi:\n",
        },
        capsys,
    )
    assert lines == [
        f"{tmp_path / 'pkg.yaml'}:3:5: error: expected ',' or ']', but got ':'",
        f"{tmp_path / 'pkg.yaml'}:2:10: note: while parsing a flow sequence",
        f"{tmp_path / 'test.yaml'}:5:3: note: included from here",
    ]


def test_secret_syntax_error_in_package(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    (tmp_path / "sub").mkdir()
    lines = _read(
        tmp_path,
        {
            "test.yaml": (
                "esphome:\n  name: test\nhost:\npackages:\n  p: !include sub/pkg.yaml\n"
            ),
            "sub/pkg.yaml": "logger:\n  level: !secret lvl\n",
            "secrets.yaml": "lvl: [1, 2\nx: 1\n",
        },
        capsys,
    )
    assert lines == [
        f"{tmp_path / 'secrets.yaml'}:2:2: error: expected ',' or ']', but got ':'",
        f"{tmp_path / 'secrets.yaml'}:1:6: note: while parsing a flow sequence",
        f"{tmp_path / 'test.yaml'}:5:3: note: included from here",
    ]


def test_undefined_secret(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    lines = _read(
        tmp_path,
        {
            "test.yaml": "esphome:\n  name: !secret nope\nhost:\n",
            "secrets.yaml": "other: 1\n",
        },
        capsys,
    )
    assert lines == [f"{tmp_path / 'test.yaml'}:2:9: error: Secret 'nope' not defined"]


def test_missing_file(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    CORE.config_path = tmp_path / "missing.yaml"
    CORE.error_format = ErrorFormat.LINE
    assert read_config({}) is None
    lines = capsys.readouterr().out.splitlines()
    assert len(lines) == 1
    assert lines[0].startswith(
        f"{tmp_path / 'missing.yaml'}: error: Error reading file "
    )


def test_source_trace_omitted(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    lines = _read(
        tmp_path,
        {
            "test.yaml": (
                "esphome:\n  name: test\nhost:\n"
                "packages:\n  p: !include ${ undefined_var }.yaml\n"
            ),
        },
        capsys,
    )
    # The "In: packages->p in test.yaml 5:3" trace would repeat the location
    assert lines == [
        (
            f"{tmp_path / 'test.yaml'}:5:3: error: Error including file "
            "'${ undefined_var }.yaml': Cannot load include with unresolved "
            "substitutions: ${ undefined_var }.yaml."
        ),
    ]


def test_print_line_message_skips_blank_lines(
    capsys: pytest.CaptureFixture[str],
) -> None:
    _print_line_message("a.yaml:1:1", "error", "first\n\n  second  \n")
    assert capsys.readouterr().out.splitlines() == [
        "a.yaml:1:1: error: first",
        "a.yaml:1:1: note: second",
    ]


def test_reset_restores_default_format() -> None:
    CORE.error_format = ErrorFormat.LINE
    CORE.reset()
    assert CORE.error_format is ErrorFormat.YAML


@pytest.mark.parametrize(
    "error",
    [
        KeyInvalid("Unable to find action", ["a", cv.ROOT_CONFIG_PATH, "b"]),
        ExtraKeysInvalid(
            "extra keys not allowed",
            ["a", cv.ROOT_CONFIG_PATH, "b"],
            candidates=["c"],
        ),
    ],
)
def test_add_error_keeps_error_class(error: KeyInvalid) -> None:
    res = Config()
    res.add_error(error)
    (added,) = res.errors
    assert type(added) is type(error)
    assert added.path == ["b"]
    assert getattr(added, "candidates", None) == getattr(error, "candidates", None)


def test_default_format_logs_load_error(
    tmp_path: Path,
    capsys: pytest.CaptureFixture[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    CORE.config_path = tmp_path / "missing.yaml"
    assert read_config({}) is None
    assert capsys.readouterr().out == ""
    assert "Error while reading config: Invalid YAML syntax" in caplog.text


def test_exception_as_error_message(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    # "ms" with no number makes the time period validator raise Invalid(ValueError)
    lines = _read(
        tmp_path,
        {
            "test.yaml": (
                "esphome:\n  name: test\nhost:\nsensor:\n  - platform: template\n"
                "    name: foo\n    update_interval: ms\n"
            )
        },
        capsys,
    )
    assert lines == [
        (
            f"{tmp_path / 'test.yaml'}:7:22: error: "
            "could not convert string to float: ''."
        )
    ]


def _mark(line: int, column: int) -> yaml.Mark:
    return yaml.Mark("a.yaml", 0, line, column, None, None)


@pytest.mark.parametrize(
    ("error", "expected"),
    [
        (
            yaml.MarkedYAMLError(
                problem="bad value", problem_mark=_mark(2, 1), note="a hint"
            ),
            ["a.yaml:3:2: error: bad value", "a.yaml:3:2: note: a hint"],
        ),
        # No problem or context: the whole error text is used, note included
        (
            yaml.MarkedYAMLError(problem_mark=_mark(2, 1), note="a hint"),
            [
                'a.yaml:3:2: error: in "a.yaml", line 3, column 2',
                "a.yaml:3:2: note: a hint",
            ],
        ),
    ],
)
def test_marked_yaml_error_note(
    capsys: pytest.CaptureFixture[str],
    error: yaml.MarkedYAMLError,
    expected: list[str],
) -> None:
    _print_marked_yaml_error(error)
    assert capsys.readouterr().out.splitlines() == expected


def test_error_without_range_gives_config_path(
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = Path("main.yaml")
    res = Config()
    res.add_error(vol.Invalid("bad value", ["sensor", 0, "name"]))
    _print_line_errors(res)
    assert capsys.readouterr().out.splitlines() == [
        "main.yaml: error: bad value.",
        "main.yaml: note: In: sensor->0->name",
    ]


def test_load_error_with_unprintable_cause(
    capsys: pytest.CaptureFixture[str],
) -> None:
    class Unprintable(Exception):
        def __str__(self) -> str:
            raise UnicodeDecodeError("utf-8", b"\xff", 0, 1, "invalid start byte")

    CORE.config_path = Path("main.yaml")
    _print_line_load_error(InvalidYAMLError(Unprintable()))
    assert capsys.readouterr().out.splitlines() == ["main.yaml: error: Unprintable()"]
