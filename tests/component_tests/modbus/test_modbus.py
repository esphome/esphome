"""Tests for modbus configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components import modbus
from esphome.components.modbus import CONF_MODBUS_ID, _validate_server_address
from esphome.const import CONF_ADDRESS


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
    # Not rejected for clients today, but not supported either: a client broadcast gets no reply and
    # stalls the hub for the full send-wait.
    schema = modbus.modbus_device_schema(0x01)
    assert schema({CONF_MODBUS_ID: "hub", CONF_ADDRESS: 0})[CONF_ADDRESS] == 0
