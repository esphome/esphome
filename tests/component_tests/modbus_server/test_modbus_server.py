"""Tests for modbus_server configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.modbus_server import _validate_no_overlapping_registers
from esphome.components.modbus_server.const import CONF_REGISTERS
from esphome.const import CONF_ADDRESS, CONF_VALUE_TYPE


def _config(registers: list[tuple[int, str]]) -> dict:
    return {
        CONF_REGISTERS: [
            {CONF_ADDRESS: address, CONF_VALUE_TYPE: value_type}
            for address, value_type in registers
        ]
    }


def test_non_overlapping_registers_pass() -> None:
    # Values that tile the address space without gaps or overlaps are accepted.
    config = _config([(0x00, "U_WORD"), (0x01, "U_DWORD"), (0x03, "U_WORD")])
    assert _validate_no_overlapping_registers(config) is config


def test_registers_with_gaps_pass() -> None:
    config = _config([(0x00, "U_WORD"), (0x05, "U_QWORD"), (0x20, "U_WORD")])
    assert _validate_no_overlapping_registers(config) is config


def test_no_registers_pass() -> None:
    assert _validate_no_overlapping_registers({}) == {}


def test_duplicate_address_rejected() -> None:
    config = _config([(0x10, "U_WORD"), (0x10, "U_WORD")])
    with pytest.raises(cv.Invalid, match="overlaps"):
        _validate_no_overlapping_registers(config)


def test_multi_register_value_overlapping_neighbour_rejected() -> None:
    # U_DWORD at 0x10 occupies 0x10 and 0x11; a U_WORD at 0x11 collides with its low word.
    config = _config([(0x10, "U_DWORD"), (0x11, "U_WORD")])
    with pytest.raises(cv.Invalid, match="overlaps"):
        _validate_no_overlapping_registers(config)


def test_overlap_detected_regardless_of_order() -> None:
    # The U_DWORD at 0x10 covers 0x10-0x11 and overlaps the U_WORD at 0x11 even when declared after it.
    config = _config([(0x11, "U_WORD"), (0x10, "U_DWORD")])
    with pytest.raises(cv.Invalid, match="overlaps"):
        _validate_no_overlapping_registers(config)
