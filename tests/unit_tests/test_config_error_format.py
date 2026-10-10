"""Tests for the `--error-format line` output of read_config."""

from pathlib import Path

import pytest
import voluptuous as vol
import yaml

from esphome.config import (
    Config,
    _print_line_errors,
    _print_line_load_error,
    _print_line_message,
    read_config,
)
from esphome.const import ErrorFormat
from esphome.core import CORE, EsphomeError


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
    # validate_registry_entry's "Unable to find <kind> with the name '<key>'"
    # is raised as Invalid(msg, [key]) -- a path ending on the unrecognized
    # key itself, same as ExtraKeysInvalid. The location must land on that
    # key, not on the action's value (the previous behavior).
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
        f"{tmp_path / 'test.yaml'}:5:3: note: Error including file 'pkg.yaml'",
        (
            f"{tmp_path / 'test.yaml'}:5:3: note: In: packages->pkg in "
            f"{tmp_path / 'test.yaml'} 5:3"
        ),
    ]


def test_secret_error_keeps_earlier_failure(
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
    assert lines[0] == (
        f"{tmp_path / 'secrets.yaml'}:2:2: error: expected ',' or ']', but got ':'"
    )
    # The missing package-local secrets file is reported, not dropped
    assert f"Error reading file {tmp_path / 'sub' / 'secrets.yaml'}" in lines[2]


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


def test_multi_line_message(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
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
    location = f"{tmp_path / 'test.yaml'}:5:3"
    assert lines == [
        (
            f"{location}: error: Error including file '${{ undefined_var }}.yaml': "
            "Cannot load include with unresolved substitutions: "
            "${ undefined_var }.yaml"
        ),
        f"{location}: note: In: packages->p in {tmp_path / 'test.yaml'} 5:3.",
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


def _yaml_error() -> yaml.MarkedYAMLError:
    with pytest.raises(yaml.MarkedYAMLError) as exc_info:
        yaml.safe_load("a: [1, 2\nb: 1\n")
    return exc_info.value


def test_load_error_keeps_wrapper_context(
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = Path("main.yaml")
    yaml_err = _yaml_error()
    err = EsphomeError(f"First file failed\n{yaml_err}")
    err.__cause__ = yaml_err
    _print_line_load_error(err)
    assert capsys.readouterr().out.splitlines() == [
        "<unicode string>:2:2: error: expected ',' or ']', but got ':'",
        "<unicode string>:1:4: note: while parsing a flow sequence",
        "main.yaml: note: First file failed",
    ]


def test_include_error_without_wrapper_text(
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = Path("main.yaml")
    yaml_err = _yaml_error()
    err = vol.Invalid(str(yaml_err), [])
    err.__cause__ = yaml_err
    res = Config()
    res.errors = [err]
    _print_line_errors(res)
    assert capsys.readouterr().out.splitlines()[-1] == (
        "main.yaml: note: included from here"
    )


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
