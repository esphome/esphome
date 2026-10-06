"""Tests for modbus configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components import modbus
from esphome.components.modbus import (
    _HUB_TIME_PERIOD,
    CONF_MODBUS_ID,
    _validate_server_address,
)
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
    # A client may address 0: writes are broadcast, and reads are allowed with allow_broadcast_read.
    schema = modbus.modbus_device_schema(0x01)
    assert schema({CONF_MODBUS_ID: "hub", CONF_ADDRESS: 0})[CONF_ADDRESS] == 0


def test_hub_time_accepts_up_to_65535_ms() -> None:
    assert _HUB_TIME_PERIOD("65535ms").total_milliseconds == 65535


def test_hub_time_rejects_values_the_hub_would_truncate() -> None:
    # The setters take 16-bit milliseconds: 70 s would silently become 4464 ms.
    with pytest.raises(cv.Invalid):
        _HUB_TIME_PERIOD("70s")


def test_on_request_rejects_a_deferring_action() -> None:
    # The request PDU is only valid while the handler runs.
    validator = modbus.synchronous_handler("modbus")
    with pytest.raises(cv.Invalid, match="not allowed in modbus handlers"):
        validator({"then": [{"delay": 1000}]})


def test_on_request_accepts_synchronous_actions() -> None:
    validator = modbus.synchronous_handler("modbus")
    config = {"then": [{"lambda": "return;"}]}
    assert validator(config) is config


def test_on_request_registers_a_callback(generate_main) -> None:
    main_cpp = generate_main("tests/component_tests/modbus/test_on_request.yaml")
    assert "client_port->add_on_request_callback(" in main_cpp
    # A server hub without a handler gets no callback.
    assert "plain_server->add_on_request_callback(" not in main_cpp
