"""Tests for modbus configuration validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import modbus
from esphome.components.modbus import (
    _HUB_TIME_PERIOD,
    CONF_MODBUS_ID,
    _validate_server_address,
)
from esphome.config import read_config
from esphome.const import CONF_ADDRESS
from esphome.core import CORE


def test_server_address_accepts_valid_unit_address() -> None:
    # A normal unit address (1-247) is accepted and returned as an int.
    assert _validate_server_address(1) == 1
    assert _validate_server_address(247) == 247


def test_server_address_accepts_hex_string() -> None:
    # hex_uint8_t parses hex strings, and the validator returns the parsed int.
    assert _validate_server_address("0x10") == 0x10


def test_server_address_zero_rejected() -> None:
    # Address 0 is the Modbus broadcast address and cannot identify a server device.
    with pytest.raises(cv.Invalid, match="broadcast address"):
        _validate_server_address(0)


def test_server_schema_rejects_address_zero() -> None:
    # The server-role schema wires in _validate_server_address, so address 0 is rejected there too.
    schema = modbus.modbus_device_schema(0x01, role="server")
    with pytest.raises(cv.Invalid, match="broadcast address"):
        schema({CONF_MODBUS_ID: "hub", CONF_ADDRESS: 0})


def test_client_schema_still_accepts_address_zero() -> None:
    # A client may address 0: writes are broadcast, and reads are allowed with allow_broadcast_read.
    schema = modbus.modbus_device_schema(0x01)
    assert schema({CONF_MODBUS_ID: "hub", CONF_ADDRESS: 0})[CONF_ADDRESS] == 0


def test_hub_time_accepts_up_to_65535_ms() -> None:
    assert _HUB_TIME_PERIOD("65535ms").total_milliseconds == 65535


def test_hub_time_rejects_values_the_hub_would_truncate() -> None:
    # The setters take 16-bit milliseconds: 70 s would silently become 4464 ms.
    with pytest.raises(cv.Invalid):
        _HUB_TIME_PERIOD("70s")


def test_device_without_modbus_block_names_the_fix(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    # The pzemac AUTO_LOAD of modbus no longer creates a hub, so the error has to point at the missing block.
    CORE.config_path = component_config_path("no_hub.yaml")
    assert read_config({}) is None
    assert "Add a 'modbus:' block" in capsys.readouterr().out


def test_empty_modbus_block_creates_one_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("empty_hub_block.yaml"))
    assert main_cpp.count("static modbus::ModbusClientHub *const") == 1
