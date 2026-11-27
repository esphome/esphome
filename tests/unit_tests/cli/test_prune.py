"""Tests for ``esphome prune``."""

from __future__ import annotations

from dataclasses import dataclass
import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.__main__ import parse_args
from esphome.cli.prune import _default_idf_versions, command_prune
from esphome.components.esp32 import (
    ARDUINO_FRAMEWORK_VERSION_LOOKUP,
    ARDUINO_IDF_VERSION_LOOKUP,
    ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
)
from esphome.core import EsphomeError


@dataclass
class MockArgs:
    days: int = 30
    dry_run: bool = False


def test_default_idf_versions() -> None:
    versions = _default_idf_versions()

    assert str(ESP_IDF_FRAMEWORK_VERSION_LOOKUP["recommended"]) in versions
    arduino = ARDUINO_FRAMEWORK_VERSION_LOOKUP["recommended"]
    assert str(ARDUINO_IDF_VERSION_LOOKUP[arduino]) in versions


def test_command_prune_passes_arguments() -> None:
    with patch(
        "esphome.espidf.prune.prune_idf_installs", return_value=[]
    ) as mock_prune:
        assert command_prune(MockArgs(days=7, dry_run=True)) == 0

    mock_prune.assert_called_once_with(_default_idf_versions(), 7, dry_run=True)


@pytest.mark.parametrize(
    ("removed", "dry_run", "expected"),
    [
        ([], False, "Nothing to prune"),
        ([], True, "Nothing to prune"),
        ([Path("frameworks/5.5.6")], False, "Done!"),
        ([Path("frameworks/5.5.6")], True, None),
    ],
)
def test_command_prune_reports_outcome(
    removed: list[Path],
    dry_run: bool,
    expected: str | None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        caplog.at_level(logging.INFO),
        patch("esphome.espidf.prune.prune_idf_installs", return_value=removed),
    ):
        assert command_prune(MockArgs(dry_run=dry_run)) == 0

    assert caplog.messages == ([expected] if expected else [])


def test_command_prune_oserror(caplog: pytest.LogCaptureFixture) -> None:
    with patch(
        "esphome.espidf.prune.prune_idf_installs",
        side_effect=PermissionError("denied"),
    ):
        assert command_prune(MockArgs()) == 1

    assert "Error pruning ESP-IDF installs: denied" in caplog.text


def test_command_prune_rejects_negative_days() -> None:
    with (
        patch("esphome.espidf.prune.prune_idf_installs") as mock_prune,
        pytest.raises(EsphomeError, match="must not be negative"),
    ):
        command_prune(MockArgs(days=-1))

    mock_prune.assert_not_called()


def test_parse_args_prune() -> None:
    args = parse_args(["esphome", "prune"])
    assert (args.command, args.days, args.dry_run) == ("prune", 30, False)

    args = parse_args(["esphome", "prune", "--days", "0", "--dry-run"])
    assert (args.days, args.dry_run) == (0, True)
