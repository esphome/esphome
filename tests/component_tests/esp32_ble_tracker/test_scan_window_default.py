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
import logging
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.ble_device_base import CONF_CONNECTION_SCAN_WINDOW, to_ble_units
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


# The connection-time fallback window: while a GATT connection is active the
# scanner drops from a raised full-duty window back to this value so the
# connection gets guaranteed airtime.


def test_raise_arms_connection_scan_window_default(
    stage_esp32: Callable[..., None],
) -> None:
    stage_esp32("5.5.5", wifi=True)
    params = _scan_params({})
    assert params[CONF_WINDOW] == params[CONF_INTERVAL]
    assert to_ble_units(params[CONF_CONNECTION_SCAN_WINDOW]) == 48


def test_user_connection_scan_window_survives_raise(
    stage_esp32: Callable[..., None],
) -> None:
    stage_esp32("5.5.5", wifi=True)
    params = _scan_params({"scan_parameters": {"connection_scan_window": "60ms"}})
    assert params[CONF_WINDOW] == params[CONF_INTERVAL]
    assert to_ble_units(params[CONF_CONNECTION_SCAN_WINDOW]) == 96


def test_unraised_window_gets_no_connection_scan_window_default(
    stage_esp32: Callable[..., None],
) -> None:
    stage_esp32("5.5.4", wifi=True)
    assert CONF_CONNECTION_SCAN_WINDOW not in _scan_params({})


def test_connection_scan_window_above_interval_rejected(
    stage_esp32: Callable[..., None],
) -> None:
    stage_esp32("5.5.5", wifi=True)
    with pytest.raises(
        cv.Invalid, match="connection_scan_window .* needs to be smaller"
    ):
        _scan_params({"scan_parameters": {"connection_scan_window": "400ms"}})


def test_connection_scan_window_above_window_rejected(
    stage_esp32: Callable[..., None],
) -> None:
    """A connection window above the (post-raise) window would widen the scan
    during connections; the reject runs after the raise so a fallback below a
    raised window still validates (covered by the survives-raise test)."""
    stage_esp32("5.5.5", wifi=True)
    with pytest.raises(
        cv.Invalid, match="connection_scan_window .* needs to be smaller"
    ):
        _scan_params(
            {"scan_parameters": {"window": "30ms", "connection_scan_window": "300ms"}}
        )


def test_connection_scan_window_truncation_collapse_rejected(
    stage_esp32: Callable[..., None],
) -> None:
    """A connection window that truncates into the interval's 0.625 ms unit
    would silently program a full-duty scan during connections."""
    stage_esp32("5.5.5", wifi=True)
    with pytest.raises(cv.Invalid, match="connection_scan_window .* both truncate"):
        _scan_params(
            {
                "scan_parameters": {
                    "interval": "320.5ms",
                    "connection_scan_window": "320.2ms",
                }
            }
        )


@pytest.mark.parametrize(
    ("config_file", "window_call", "connection_call", "warns"),
    [
        # Raised window with GATT clients: the injected fallback is emitted.
        ("scan_window_raised.yaml", "set_scan_window(512)", True, False),
        # Explicit window: nothing injected.
        ("scan_window_explicit.yaml", "set_scan_window(48)", False, False),
        # Scan-only build compiles the path out: the injected default is
        # dropped silently, a user-set value warns.
        ("scan_window_scan_only.yaml", "set_scan_window(512)", False, False),
        ("scan_window_user_set_scan_only.yaml", "set_scan_window(512)", False, True),
    ],
)
def test_connection_scan_window_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
    config_file: str,
    window_call: str,
    connection_call: bool,
    warns: bool,
) -> None:
    main_cpp = generate_main(component_config_path(config_file))
    assert window_call in main_cpp
    assert ("set_connection_scan_window(48)" in main_cpp) == connection_call
    assert ("'connection_scan_window' has no effect" in caplog.text) == warns


@pytest.mark.parametrize(
    ("wifi", "params", "expect_warning"),
    [
        (True, {"interval": "1100ms", "window": "1100ms"}, True),
        (True, {"interval": "1100ms", "window": "601ms"}, True),
        (True, {"interval": "1100ms", "window": "600ms"}, False),
        (False, {"interval": "1100ms", "window": "1100ms"}, False),
    ],
)
def test_long_window_with_wifi_warns(
    stage_esp32: Callable[..., None],
    caplog: pytest.LogCaptureFixture,
    wifi: bool,
    params: ConfigType,
    expect_warning: bool,
) -> None:
    """A scan window above 600 ms warns only when wifi shares the radio."""
    stage_esp32("5.5.5", wifi=wifi)
    with caplog.at_level(logging.WARNING):
        _scan_params({"scan_parameters": params})
    assert ("starves wifi" in caplog.text) is expect_warning


def test_long_window_warns_with_coexistence_disabled(
    stage_esp32: Callable[..., None],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Disabling the arbiter is the worst case for a long window, so it still warns."""
    stage_esp32("5.5.5", wifi=True)
    with caplog.at_level(logging.WARNING):
        _scan_params(
            {
                CONF_SOFTWARE_COEXISTENCE: False,
                "scan_parameters": {"interval": "1100ms", "window": "1100ms"},
            }
        )
    assert "BLE scan window of 1100ms" in caplog.text


def test_raised_window_warning_points_at_interval(
    stage_esp32: Callable[..., None],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """When the window was raised to a long interval, the warning names the interval."""
    stage_esp32("5.5.5", wifi=True)
    with caplog.at_level(logging.WARNING):
        _scan_params({"scan_parameters": {"interval": "1s"}})
    assert "BLE scan interval of 1s" in caplog.text
    assert "BLE scan window of" not in caplog.text
