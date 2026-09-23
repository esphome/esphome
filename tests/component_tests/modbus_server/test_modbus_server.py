"""Tests for modbus_server configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.modbus_server import (
    SERVER_SENSOR_VALUE_TYPE,
    _validate_no_overlapping_registers,
    _validate_register_ranges,
)
from esphome.components.modbus_server.const import CONF_REGISTERS, CONF_VALUE_TYPE
from esphome.const import CONF_ADDRESS


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


def test_register_span_within_address_space_pass() -> None:
    # A value whose span ends exactly at 0xFFFF is fine (U_QWORD at 0xFFFC covers 0xFFFC-0xFFFF).
    config = _config([(0xFFFF, "U_WORD"), (0xFFFC, "U_QWORD")])
    assert _validate_register_ranges(config) is config


def test_register_span_past_end_rejected() -> None:
    # U_QWORD at 0xFFFE would need 0xFFFE-0x10001, running off the 16-bit address space.
    config = _config([(0xFFFE, "U_QWORD")])
    with pytest.raises(cv.Invalid, match="past the end"):
        _validate_register_ranges(config)


def test_multi_register_value_at_last_address_rejected() -> None:
    # A U_DWORD at 0xFFFF needs a second register at 0x10000, which does not exist.
    config = _config([(0xFFFF, "U_DWORD")])
    with pytest.raises(cv.Invalid, match="past the end"):
        _validate_register_ranges(config)


def test_raw_value_type_rejected() -> None:
    # RAW has no numeric encoding, so it is not offered as a server register type.
    validator = cv.enum(SERVER_SENSOR_VALUE_TYPE)
    with pytest.raises(cv.Invalid):
        validator("RAW")
    assert validator("U_WORD") == "U_WORD"
