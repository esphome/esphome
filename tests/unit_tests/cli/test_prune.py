"""Tests for ``esphome prune``."""

from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
import logging
from pathlib import Path
from unittest.mock import Mock, patch

import pytest

from esphome.__main__ import PRE_CONFIG_ACTIONS, parse_args
from esphome.arduino8266.framework import FRAMEWORK_RELEASES
from esphome.cli.prune import (
    _recommended_arduino8266_frameworks,
    _recommended_idf_versions,
    _recommended_platformio_sources,
    _recommended_sdk_nrf_versions,
    command_prune,
)
from esphome.components.esp32 import (
    ARDUINO_FRAMEWORK_VERSION_LOOKUP,
    ARDUINO_IDF_VERSION_LOOKUP,
    ESP_IDF_FRAMEWORK_VERSION_LOOKUP,
)
from esphome.components.esp8266 import RECOMMENDED_ARDUINO_FRAMEWORK_VERSION
from esphome.components.libretiny import ARDUINO_VERSIONS
from esphome.components.nrf52 import MIN_SDK_NRF_VERSIONS, RECOMMENDED_SDK_NRF_VERSION
from esphome.components.nrf52.const import VARIANT_NRF54LM20
from esphome.components.rp2 import RECOMMENDED_ARDUINO_PLATFORM_VERSION
from esphome.platformio.prune import DEPRECATED_PLATFORMS


@dataclass
class MockArgs:
    dry_run: bool = False


_PRUNERS = {
    "idf": "esphome.espidf.prune.prune_idf_installs",
    "arduino8266": "esphome.arduino8266.prune.prune_arduino8266_installs",
    "sdk_nrf": "esphome.components.nrf52.prune.prune_sdk_nrf_installs",
    "platformio": "esphome.platformio.prune.prune_platformio_installs",
}


@pytest.fixture
def pruners() -> Generator[dict[str, Mock]]:
    """Every backend's prune, each removing nothing."""
    patches = {
        name: patch(target, return_value=[]) for name, target in _PRUNERS.items()
    }
    mocks = {name: patcher.start() for name, patcher in patches.items()}
    yield mocks
    for patcher in patches.values():
        patcher.stop()


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


def test_recommended_platformio_sources() -> None:
    sources = _recommended_platformio_sources()

    assert set(sources) == {"raspberrypi", "libretiny"}
    assert RECOMMENDED_ARDUINO_PLATFORM_VERSION in sources["raspberrypi"]
    assert sources["libretiny"] == ARDUINO_VERSIONS["recommended"][1]


def test_command_prune_passes_arguments(pruners: dict[str, Mock]) -> None:
    assert command_prune(MockArgs(dry_run=True)) == 0

    pruners["idf"].assert_called_once_with(_recommended_idf_versions(), dry_run=True)
    pruners["arduino8266"].assert_called_once_with(
        _recommended_arduino8266_frameworks(), dry_run=True
    )
    pruners["sdk_nrf"].assert_called_once_with(
        _recommended_sdk_nrf_versions(), dry_run=True
    )
    pruners["platformio"].assert_called_once_with(
        DEPRECATED_PLATFORMS, _recommended_platformio_sources(), dry_run=True
    )


@pytest.mark.parametrize(
    ("removed_by", "dry_run", "expected"),
    [
        (None, False, "Nothing to prune"),
        (None, True, "Nothing to prune"),
        ("idf", False, "Done!"),
        ("arduino8266", False, "Done!"),
        ("sdk_nrf", False, "Done!"),
        ("platformio", False, "Done!"),
        ("idf", True, None),
    ],
)
def test_command_prune_reports_outcome(
    removed_by: str | None,
    dry_run: bool,
    expected: str | None,
    pruners: dict[str, Mock],
    caplog: pytest.LogCaptureFixture,
) -> None:
    if removed_by is not None:
        pruners[removed_by].return_value = [Path("removed")]

    with caplog.at_level(logging.INFO):
        assert command_prune(MockArgs(dry_run=dry_run)) == 0

    assert caplog.messages == ([expected] if expected else [])


def test_command_prune_oserror(
    pruners: dict[str, Mock], caplog: pytest.LogCaptureFixture
) -> None:
    pruners["idf"].side_effect = PermissionError("denied")

    assert command_prune(MockArgs()) == 1
    assert "Error pruning installs: denied" in caplog.text


def test_prune_is_dispatched_without_a_configuration(pruners: dict[str, Mock]) -> None:
    args = parse_args(["esphome", "prune", "--dry-run"])

    assert PRE_CONFIG_ACTIONS[args.command](args) == 0
    pruners["idf"].assert_called_once_with(_recommended_idf_versions(), dry_run=True)


def test_parse_args_prune() -> None:
    args = parse_args(["esphome", "prune"])
    assert (args.command, args.dry_run) == ("prune", False)

    assert parse_args(["esphome", "prune", "--dry-run"]).dry_run is True
