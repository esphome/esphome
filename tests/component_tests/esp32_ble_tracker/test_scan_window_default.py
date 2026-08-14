"""Tests for the esp32_ble_tracker conditional scan window default.

The scan window default depends on wifi coexistence and the IDF version:
IDF 5.5.5 fixed a coexistence bug where BLE scans ran far longer than the
configured window (espressif/esp-idf#18931), so on fixed versions the
historical 30 ms default would only listen 9.4 % of the time and miss most
advertisements. With the coexistence arbiter compiled in on a fixed IDF, the
window instead defaults to the interval, as Espressif recommends; without the
arbiter a full-duty scan would starve wifi, so the 30 ms default is kept.
"""

from __future__ import annotations

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.ble_device_base import to_ble_units
from esphome.components.const import CONF_SCAN_PARAMETERS, CONF_WINDOW
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_ble_tracker import (
    CONF_SOFTWARE_COEXISTENCE,
    CONFIG_SCHEMA,
)
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
            # Makes cv.OnlyWith default software_coexistence to True, exactly
            # as a real config with wifi: does.
            CORE.loaded_integrations.add("wifi")

    return stage


def _scan_params(config: ConfigType) -> ConfigType:
    return CONFIG_SCHEMA(config)[CONF_SCAN_PARAMETERS]


@pytest.mark.parametrize(
    ("idf", "config", "expected_units"),
    [
        ("5.5.5", {}, 512),  # first fixed version, default 320 ms interval
        ("6.0.1", {}, 512),  # any newer version behaves the same
        # Follows a user-set interval.
        ("5.5.5", {"scan_parameters": {"interval": "1s"}}, 1600),
    ],
)
def test_wifi_on_fixed_idf_defaults_window_to_interval(
    stage_esp32: Callable[..., None],
    idf: str,
    config: ConfigType,
    expected_units: int,
) -> None:
    """With wifi coexistence on a fixed IDF, the window defaults to the interval."""
    stage_esp32(idf, wifi=True)
    params = _scan_params(config)
    assert params[CONF_WINDOW] == params[CONF_INTERVAL]
    assert to_ble_units(params[CONF_WINDOW]) == expected_units


@pytest.mark.parametrize(
    ("idf", "wifi", "config"),
    [
        # Buggy IDF over-scans anyway; keep the 30 ms default.
        ("5.5.4", True, {}),
        # No wifi (e.g. ethernet) means no radio contention.
        ("5.5.5", False, {}),
        # Coexistence disabled: no arbiter, so a full-duty scan would starve
        # wifi outright.
        ("5.5.5", True, {CONF_SOFTWARE_COEXISTENCE: False}),
    ],
)
def test_30ms_default_kept(
    stage_esp32: Callable[..., None],
    idf: str,
    wifi: bool,
    config: ConfigType,
) -> None:
    stage_esp32(idf, wifi=wifi)
    assert to_ble_units(_scan_params(config)[CONF_WINDOW]) == 48


@pytest.mark.parametrize("window", ["60ms", "30ms"])
def test_explicit_window_is_never_touched(
    stage_esp32: Callable[..., None], window: str
) -> None:
    """A user-set window wins over the conditional default.

    The explicit 30 ms case matters: it is indistinguishable from the
    defaulted value by inspection, so the defaulted flag must separate them.
    """
    stage_esp32("5.5.5", wifi=True)
    params = _scan_params({"scan_parameters": {"window": window}})
    assert to_ble_units(params[CONF_WINDOW]) == to_ble_units(
        cv.positive_time_period(window)
    )


def test_short_interval_without_window_still_rejected(
    stage_esp32: Callable[..., None],
) -> None:
    """The provisional 30 ms default validates against the interval as before."""
    stage_esp32("5.5.5", wifi=True)
    with pytest.raises(cv.Invalid, match="needs to be smaller than scan interval"):
        _scan_params({"scan_parameters": {"interval": "20ms"}})
