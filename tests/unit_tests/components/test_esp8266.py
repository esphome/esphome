"""Tests for ESP8266 component."""

from __future__ import annotations

from collections.abc import Generator
from unittest.mock import MagicMock, patch

import pytest

from esphome.components import esp8266
from esphome.components.esp8266 import check_rosetta
from esphome.core import EsphomeError


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
