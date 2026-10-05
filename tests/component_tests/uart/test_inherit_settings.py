"""Tests for checking devices on a UART that runs with the settings of another one."""

import pytest

from esphome import config_validation as cv
from esphome.components import uart
from esphome.components.const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS
from esphome.config import Config
from esphome.const import CONF_BAUD_RATE, CONF_ID, CONF_UART_ID, PlatformFramework
from esphome.core import ID
from tests.component_tests.types import SetCoreConfigCallable


def _full_config() -> Config:
    """A hardware UART at 9600 baud and a UART without settings, as the ID pass leaves them."""
    full = Config()
    full["uart"] = [
        {
            CONF_ID: ID("pins"),
            CONF_BAUD_RATE: 9600,
            CONF_DATA_BITS: 8,
            CONF_PARITY: "NONE",
            CONF_STOP_BITS: 1,
        }
    ]
    full["forwarder"] = [{CONF_ID: ID("copy")}]
    full.declare_ids.append((full["uart"][0][CONF_ID], ["uart", 0, CONF_ID]))
    full.declare_ids.append((full["forwarder"][0][CONF_ID], ["forwarder", 0, CONF_ID]))
    return full


def _device(**settings) -> dict:
    return uart.final_validate_device_schema("test_device", **settings)(
        {CONF_UART_ID: ID("copy")}
    )


def test_settings_source_follows_every_hop() -> None:
    uart.inherit_settings(ID("a"), ID("b"))
    uart.inherit_settings(ID("b"), ID("pins"))
    assert str(uart._settings_source(ID("a"))) == "pins"
    assert str(uart._settings_source(ID("b"))) == "pins"
    assert uart._settings_source(ID("pins")) is None


def test_settings_source_ends_on_a_loop() -> None:
    uart.inherit_settings(ID("a"), ID("b"))
    uart.inherit_settings(ID("b"), ID("a"))
    assert str(uart._settings_source(ID("a"))) in ("a", "b")


def test_device_is_checked_against_the_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config())
    uart.inherit_settings(ID("copy"), ID("pins"))
    _device(baud_rate=9600, data_bits=8, parity="NONE", stop_bits=1)


def test_mismatch_is_reported_at_the_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config())
    uart.inherit_settings(ID("copy"), ID("pins"))
    with pytest.raises(cv.Invalid, match="requires baud rate 115200") as err:
        _device(baud_rate=115200)
    assert err.value.path[-3:] == ["uart", 0, CONF_BAUD_RATE]


def test_device_without_settings_needs_no_source(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config())
    _device()


def test_without_a_source_the_uart_needs_its_own_settings(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config())
    with pytest.raises(cv.Invalid, match="required key not provided"):
        _device(baud_rate=9600)
