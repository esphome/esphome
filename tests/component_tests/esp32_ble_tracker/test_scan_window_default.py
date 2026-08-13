"""Tests for the esp32_ble_tracker conditional scan window default.

The scan window default depends on the loaded integrations and the IDF
version: IDF 5.5.5 fixed a coexistence bug where BLE scans ran far longer
than the configured window (espressif/esp-idf#18931), so on fixed versions
the historical 30 ms default would only listen 9.4 % of the time and miss
most advertisements. With wifi sharing the radio on a fixed IDF, the window
instead defaults to the interval, as Espressif recommends.
"""

from __future__ import annotations

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.ble_device_base import to_ble_units
from esphome.components.const import CONF_WINDOW
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_ble_tracker import SCAN_PARAMETERS_SCHEMA
from esphome.const import CONF_INTERVAL, PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType

from ..types import SetCoreConfigCallable


@pytest.fixture
def stage_esp32(
    set_core_config: SetCoreConfigCallable,
) -> Callable[..., None]:
    """Stage an esp32 build with a given IDF version and wifi presence."""

    def stage(idf: str, *, wifi: bool) -> None:
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
        )
        if wifi:
            CORE.loaded_integrations.add("wifi")

    return stage


@pytest.mark.parametrize(
    ("idf", "scan_params", "expected_units"),
    [
        ("5.5.5", {}, 512),  # first fixed version, default 320 ms interval
        ("6.0.1", {}, 512),  # any newer version behaves the same
        ("5.5.5", {"interval": "1s"}, 1600),  # follows a user-set interval
    ],
)
def test_wifi_on_fixed_idf_defaults_window_to_interval(
    stage_esp32: Callable[..., None],
    idf: str,
    scan_params: ConfigType,
    expected_units: int,
) -> None:
    """With wifi on a fixed IDF, the window defaults to the interval."""
    stage_esp32(idf, wifi=True)
    params = SCAN_PARAMETERS_SCHEMA(scan_params)
    assert params[CONF_WINDOW] == params[CONF_INTERVAL]
    assert to_ble_units(params[CONF_WINDOW]) == expected_units


@pytest.mark.parametrize(
    ("idf", "wifi"),
    [
        ("5.5.4", True),  # buggy IDF over-scans anyway; keep the 30 ms default
        ("5.5.5", False),  # no wifi (e.g. ethernet) means no radio contention
    ],
)
def test_30ms_default_kept(
    stage_esp32: Callable[..., None], idf: str, wifi: bool
) -> None:
    stage_esp32(idf, wifi=wifi)
    assert to_ble_units(SCAN_PARAMETERS_SCHEMA({})[CONF_WINDOW]) == 48


def test_explicit_window_is_never_touched(
    stage_esp32: Callable[..., None],
) -> None:
    """A user-set window wins over the conditional default."""
    stage_esp32("5.5.5", wifi=True)
    params = SCAN_PARAMETERS_SCHEMA({"window": "60ms"})
    assert to_ble_units(params[CONF_WINDOW]) == 96


def test_filled_default_is_validated(stage_esp32: Callable[..., None]) -> None:
    """The filled-in default still goes through the shared window validation."""
    # A user interval below the 30 ms fallback makes the filled-in default
    # invalid; the shared validator runs after the defaulter and must catch it.
    stage_esp32("5.5.5", wifi=False)
    with pytest.raises(cv.Invalid, match="needs to be smaller than scan interval"):
        SCAN_PARAMETERS_SCHEMA({"interval": "20ms"})
