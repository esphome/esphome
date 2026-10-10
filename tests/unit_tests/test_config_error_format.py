"""Tests for the `--error-format line` output of read_config."""

from pathlib import Path

import pytest

from esphome.config import _print_line_message, read_config
from esphome.const import ErrorFormat
from esphome.core import CORE


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
