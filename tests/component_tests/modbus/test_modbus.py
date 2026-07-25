"""Tests for modbus configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.modbus import _validate_server_address


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
