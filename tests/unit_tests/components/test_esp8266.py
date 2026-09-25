"""Tests for ESP8266 component."""

from __future__ import annotations

from collections.abc import Generator
from unittest.mock import MagicMock, patch

import pytest

from esphome.components import esp8266
from esphome.components.esp8266 import check_rosetta, lambdas_use_scanf_float
from esphome.core import EsphomeError, Lambda
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("src", "expected"),
    [
        # Basic float formats
        ('sscanf(buf, "%f", &v)', True),
        ('sscanf(buf, "%F", &v)', True),
        ('sscanf(buf, "%e", &v)', True),
        ('sscanf(buf, "%E", &v)', True),
        ('sscanf(buf, "%g", &v)', True),
        ('sscanf(buf, "%G", &v)', True),
        ('sscanf(buf, "%a", &v)', True),
        ('sscanf(buf, "%A", &v)', True),
        # With modifiers
        ('sscanf(buf, "%lf", &v)', True),
        ('sscanf(buf, "%Lf", &v)', True),
        ('sscanf(buf, "%8lf", &v)', True),
        ('sscanf(buf, "%*f")', True),
        ('sscanf(buf, "%.2f", &v)', True),
        # Mixed formats
        ('sscanf(buf, "%d,%f", &a, &b)', True),
        # fscanf and std::sscanf
        ('fscanf(fp, "%f", &v)', True),
        ('std::sscanf(buf, "%f", &v)', True),
        # Multi-line
        ('sscanf(buf,\n"%f", &v)', True),
        # No float format
        ('sscanf(buf, "%d", &v)', False),
        ('sscanf(buf, "%s", s)', False),
        # printf not scanf
        ('printf("%f", val)', False),
        # %f in a different statement after scanf
        ('sscanf(buf, "%d", &x); printf("%f", val);', False),
        # scanf %f in comment only
        ('// sscanf(buf, "%f", &v)\nsscanf(buf, "%d", &x)', False),
        ('/* sscanf(buf, "%f") */\nsscanf(buf, "%d", &x)', False),
    ],
)
def test_lambdas_use_scanf_float(src: str, expected: bool) -> None:
    """Test scanf float detection in lambda source."""
    config: ConfigType = {"test": [Lambda(src)]}
    assert lambdas_use_scanf_float(config) is expected


def test_lambdas_use_scanf_float_no_lambdas() -> None:
    """Test with config containing no lambdas."""
    config: ConfigType = {"key": "value", "list": [1, 2]}
    assert lambdas_use_scanf_float(config) is False


def test_lambdas_use_scanf_float_nested() -> None:
    """Test detection in deeply nested config."""
    config: ConfigType = {"a": {"b": {"c": [Lambda('sscanf(buf, "%f", &v)')]}}}
    assert lambdas_use_scanf_float(config) is True


@pytest.fixture
def apple_silicon_run(monkeypatch: pytest.MonkeyPatch) -> Generator[MagicMock]:
    """Simulate an Apple Silicon Mac and yield the mocked subprocess.run."""
    monkeypatch.setattr(esp8266, "IS_MACOS", True)
    with (
        patch("esphome.components.esp8266.platform.machine", return_value="arm64"),
        patch("esphome.components.esp8266.subprocess.run") as mock_run,
    ):
        yield mock_run


@pytest.mark.parametrize(
    ("is_macos", "machine"),
    [
        (False, "arm64"),
        (True, "x86_64"),
    ],
)
def test_check_rosetta_skips_other_systems(
    monkeypatch: pytest.MonkeyPatch, is_macos: bool, machine: str
) -> None:
    """The check only probes on Apple Silicon Macs."""
    monkeypatch.setattr(esp8266, "IS_MACOS", is_macos)
    with (
        patch("esphome.components.esp8266.platform.machine", return_value=machine),
        patch("esphome.components.esp8266.subprocess.run") as mock_run,
    ):
        check_rosetta()
    mock_run.assert_not_called()


def test_check_rosetta_installed(apple_silicon_run: MagicMock) -> None:
    """No error when the x86_64 probe succeeds (Rosetta present)."""
    apple_silicon_run.return_value = MagicMock(returncode=0)
    check_rosetta()
    apple_silicon_run.assert_called_once()


def test_check_rosetta_missing(apple_silicon_run: MagicMock) -> None:
    """A failing x86_64 probe raises an actionable error."""
    apple_silicon_run.return_value = MagicMock(returncode=1)
    with pytest.raises(EsphomeError, match="softwareupdate --install-rosetta"):
        check_rosetta()


def test_check_rosetta_arch_unavailable(apple_silicon_run: MagicMock) -> None:
    """The build proceeds when arch(1) cannot be executed."""
    apple_silicon_run.side_effect = OSError("no such file")
    check_rosetta()
