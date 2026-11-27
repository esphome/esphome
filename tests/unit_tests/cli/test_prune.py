"""Tests for ``esphome prune``."""

from __future__ import annotations

from dataclasses import dataclass
import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.__main__ import parse_args
from esphome.arduino8266.framework import FRAMEWORK_RELEASES
from esphome.cli.prune import (
    _recommended_arduino8266_frameworks,
    _recommended_idf_versions,
    _recommended_sdk_nrf_versions,
    command_prune,
)
from esphome.components.esp32 import (
    ARDUINO_FRAMEWORK_VERSION_LOOKUP,
    ARDUINO_IDF_VERSION_LOOKUP,
    ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
)
from esphome.components.esp8266 import RECOMMENDED_ARDUINO_FRAMEWORK_VERSION
from esphome.components.nrf52 import MIN_SDK_NRF_VERSIONS, RECOMMENDED_SDK_NRF_VERSION
from esphome.components.nrf52.const import VARIANT_NRF54LM20


@dataclass
class MockArgs:
    dry_run: bool = False


def test_recommended_idf_versions() -> None:
    arduino = ARDUINO_FRAMEWORK_VERSION_LOOKUP["recommended"]

    assert _recommended_idf_versions() == {
        str(ESP_IDF_FRAMEWORK_VERSION_LOOKUP["recommended"]),
        str(ARDUINO_IDF_VERSION_LOOKUP[arduino]),
    }


def test_recommended_arduino8266_frameworks() -> None:
    release = FRAMEWORK_RELEASES[RECOMMENDED_ARDUINO_FRAMEWORK_VERSION]

    assert _recommended_arduino8266_frameworks() == {release.tag}


def test_recommended_sdk_nrf_versions() -> None:
    """The recommended version, plus the newer one an SoC that needs it defaults to."""
    assert _recommended_sdk_nrf_versions() == {
        f"v{RECOMMENDED_SDK_NRF_VERSION}",
        f"v{MIN_SDK_NRF_VERSIONS[VARIANT_NRF54LM20]}",
    }


def test_command_prune_passes_arguments() -> None:
    with (
        patch("esphome.espidf.prune.prune_idf_installs", return_value=[]) as mock_idf,
        patch(
            "esphome.arduino8266.prune.prune_arduino8266_installs", return_value=[]
        ) as mock_arduino8266,
        patch(
            "esphome.components.nrf52.prune.prune_sdk_nrf_installs", return_value=[]
        ) as mock_sdk_nrf,
    ):
        assert command_prune(MockArgs(dry_run=True)) == 0

    mock_idf.assert_called_once_with(_recommended_idf_versions(), dry_run=True)
    mock_arduino8266.assert_called_once_with(
        _recommended_arduino8266_frameworks(), dry_run=True
    )
    mock_sdk_nrf.assert_called_once_with(_recommended_sdk_nrf_versions(), dry_run=True)


@pytest.mark.parametrize(
    ("idf", "arduino8266", "sdk_nrf", "dry_run", "expected"),
    [
        ([], [], [], False, "Nothing to prune"),
        ([], [], [], True, "Nothing to prune"),
        ([Path("frameworks/5.5.6")], [], [], False, "Done!"),
        ([], [Path("frameworks/3.1.2-esphome.3")], [], False, "Done!"),
        ([], [], [Path("frameworks/v2.6.1")], False, "Done!"),
        ([Path("frameworks/5.5.6")], [], [], True, None),
    ],
)
def test_command_prune_reports_outcome(
    idf: list[Path],
    arduino8266: list[Path],
    sdk_nrf: list[Path],
    dry_run: bool,
    expected: str | None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    with (
        caplog.at_level(logging.INFO),
        patch("esphome.espidf.prune.prune_idf_installs", return_value=idf),
        patch(
            "esphome.arduino8266.prune.prune_arduino8266_installs",
            return_value=arduino8266,
        ),
        patch(
            "esphome.components.nrf52.prune.prune_sdk_nrf_installs",
            return_value=sdk_nrf,
        ),
    ):
        assert command_prune(MockArgs(dry_run=dry_run)) == 0

    assert caplog.messages == ([expected] if expected else [])


def test_command_prune_oserror(caplog: pytest.LogCaptureFixture) -> None:
    with patch(
        "esphome.espidf.prune.prune_idf_installs",
        side_effect=PermissionError("denied"),
    ):
        assert command_prune(MockArgs()) == 1

    assert "Error pruning installs: denied" in caplog.text


def test_parse_args_prune() -> None:
    args = parse_args(["esphome", "prune"])
    assert (args.command, args.dry_run) == ("prune", False)

    assert parse_args(["esphome", "prune", "--dry-run"]).dry_run is True
