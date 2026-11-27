"""Tests for ``esphome prune``."""

from __future__ import annotations

from dataclasses import dataclass
import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.__main__ import parse_args
from esphome.cli.prune import _recommended_idf_versions, command_prune
from esphome.components.esp32 import (
    ARDUINO_FRAMEWORK_VERSION_LOOKUP,
    ARDUINO_IDF_VERSION_LOOKUP,
    ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
)


@dataclass
class MockArgs:
    dry_run: bool = False


def test_recommended_idf_versions() -> None:
    arduino = ARDUINO_FRAMEWORK_VERSION_LOOKUP["recommended"]

    assert _recommended_idf_versions() == {
        str(ESP_IDF_FRAMEWORK_VERSION_LOOKUP["recommended"]),
        str(ARDUINO_IDF_VERSION_LOOKUP[arduino]),
    }


def test_command_prune_passes_arguments() -> None:
    with patch(
        "esphome.espidf.prune.prune_idf_installs", return_value=[]
    ) as mock_prune:
        assert command_prune(MockArgs(dry_run=True)) == 0

    mock_prune.assert_called_once_with(_recommended_idf_versions(), dry_run=True)


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


def test_parse_args_prune() -> None:
    args = parse_args(["esphome", "prune"])
    assert (args.command, args.dry_run) == ("prune", False)

    assert parse_args(["esphome", "prune", "--dry-run"]).dry_run is True
