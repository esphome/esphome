"""Tests for the modbus_tcp_uart schema."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components import modbus_tcp_uart, uart
from esphome.const import CONF_ID, CONF_UART_ID, PlatformFramework
from esphome.core import ID
from tests.component_tests.helpers import uart_full_config


def test_devices_are_checked_against_the_tcp_uart_settings() -> None:
    config = modbus_tcp_uart.CONFIG_SCHEMA(
        {CONF_ID: "adapter", modbus_tcp_uart.CONF_TCP_UART_ID: "link"}
    )
    # A device such as senseair on the adapter is checked against the settings of the tcp_uart.
    assert str(uart._settings_source(config[CONF_ID])) == "link"


def _set(set_core_config, **domains) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config=uart_full_config([{CONF_ID: ID("link")}], **domains),
    )


def _adapter(adapter_id: str) -> dict:
    return {CONF_ID: ID(adapter_id), modbus_tcp_uart.CONF_TCP_UART_ID: ID("link")}


def test_a_second_adapter_on_one_link_is_rejected(set_core_config) -> None:
    _set(set_core_config)
    modbus_tcp_uart.FINAL_VALIDATE_SCHEMA(_adapter("first"))
    with pytest.raises(cv.Invalid, match="already used by another 'modbus_tcp_uart'"):
        modbus_tcp_uart.FINAL_VALIDATE_SCHEMA(_adapter("second"))


def test_a_device_reading_the_link_directly_is_rejected(set_core_config) -> None:
    _set(set_core_config, senseair=[{CONF_UART_ID: ID("link")}])
    with pytest.raises(cv.Invalid, match="also used by 'senseair'"):
        modbus_tcp_uart.FINAL_VALIDATE_SCHEMA(_adapter("first"))


def test_a_sensor_that_only_names_the_link_is_allowed(set_core_config) -> None:
    # The tcp_uart sensors read the connection state, not the bytes.
    _set(set_core_config, sensor=[{"tcp_uart_id": ID("link"), CONF_ID: ID("s")}])
    modbus_tcp_uart.FINAL_VALIDATE_SCHEMA(_adapter("first"))


def test_served_units_are_kept_in_flash(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("served_units.yaml"))

    # On ESP8266 a plain const array would sit in RAM.
    assert re.search(r"uint8_t \w+\[\] PROGMEM = \{0x05\};", main_cpp)
