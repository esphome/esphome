"""Tests for esp32_ble_server configuration helpers."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32_ble_server import (
    CCCD_DESCRIPTOR_UUID,
    CONF_ENDIANNESS,
    CONF_STRING_ENCODING,
    CUD_DESCRIPTOR_UUID,
    DEVICE_INFORMATION_SERVICE_UUID,
    float32,
    uuid_is,
    validate_descriptor_value_not_empty,
    value_bytes,
)
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_TYPE, CONF_VALUE


@pytest.mark.parametrize(
    "uuid",
    [
        DEVICE_INFORMATION_SERVICE_UUID,  # int form (cv.hex_uint32_t)
        "180A",  # 16 bit short form (bt_uuid)
        "180a",  # lowercase is normalized by bt_uuid but guard anyway
        "0000180A",  # 32 bit form
        "0000180A-0000-1000-8000-00805F9B34FB",  # full 128 bit form
    ],
)
def test_uuid_is_matches_all_representations(uuid) -> None:
    """All representations of the same 16 bit UUID must compare equal."""
    assert uuid_is(uuid, DEVICE_INFORMATION_SERVICE_UUID)


@pytest.mark.parametrize(
    "uuid",
    [
        0x1818,  # Cycling Power Service (different int)
        "1818",  # different 16 bit short form
        "0000180B",  # adjacent UUID
        "0000180A-0000-1000-8000-00805F9B34FC",  # wrong base UUID suffix
    ],
)
def test_uuid_is_rejects_other_uuids(uuid) -> None:
    """A different UUID must not be mistaken for the device information service."""
    assert not uuid_is(uuid, DEVICE_INFORMATION_SERVICE_UUID)


@pytest.mark.parametrize("uuid16", [CUD_DESCRIPTOR_UUID, CCCD_DESCRIPTOR_UUID])
def test_uuid_is_matches_descriptor_short_strings(uuid16) -> None:
    """Reserved descriptor UUIDs match whether given as int or short string."""
    assert uuid_is(uuid16, uuid16)
    assert uuid_is(f"{uuid16:04X}", uuid16)
    assert uuid_is(f"{uuid16:08X}", uuid16)


@pytest.mark.parametrize(
    ("config_file", "required"),
    [
        # Auto-loaded by improv_ble only: nothing to find until Improv asks for it
        ("improv_only.yaml", False),
        # The configuration defines a service clients are meant to connect to
        ("own_service.yaml", True),
        # Manufacturer data is only useful if it is actually broadcast
        ("manufacturer_data_only.yaml", True),
    ],
)
def test_advertising_required(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    required: bool,
) -> None:
    """The server only requests advertising when the configuration needs it."""
    main_cpp = generate_main(component_config_path(config_file))

    assert f"set_advertising_required({str(required).lower()})" in main_cpp


# The same values and bytes as tests/components/bytebuffer/test_wrap_values.cpp, which pins
# what ByteBuffer::wrap produced from the expressions this component used to emit.
@pytest.mark.parametrize(
    ("data", "type_", "endianness", "expected"),
    [
        (18, "uint8_t", "LITTLE", [0x12]),
        (18, "uint16_t", "LITTLE", [0x12, 0x00]),
        (4660, "uint16_t", "BIG", [0x12, 0x34]),
        (305419896, "uint32_t", "LITTLE", [0x78, 0x56, 0x34, 0x12]),
        (
            1311768467294899695,
            "uint64_t",
            "BIG",
            [0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF],
        ),
        (-5, "int8_t", "LITTLE", [0xFB]),
        (-2, "int16_t", "BIG", [0xFF, 0xFE]),
        (-2, "int32_t", "LITTLE", [0xFE, 0xFF, 0xFF, 0xFF]),
        (-2, "int64_t", "LITTLE", [0xFE] + [0xFF] * 7),
        (123.1, "float", "BIG", [0x42, 0xF6, 0x33, 0x33]),
        (0.1, "float", "LITTLE", [0xCD, 0xCC, 0xCC, 0x3D]),
        (0.1, "double", "LITTLE", [0x9A, 0x99, 0x99, 0x99, 0x99, 0x99, 0xB9, 0x3F]),
        (2.5, "double", "BIG", [0x40, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
    ],
)
def test_value_bytes_match_bytebuffer(
    data: float, type_: str, endianness: str, expected: list[int]
) -> None:
    config = {
        CONF_DATA: data,
        CONF_TYPE: type_,
        CONF_ENDIANNESS: endianness,
        CONF_STRING_ENCODING: "utf_8",
    }
    assert value_bytes(config) == expected


def test_value_bytes_strings_and_lists() -> None:
    assert value_bytes({CONF_DATA: "hé", CONF_STRING_ENCODING: "utf_8"}) == [
        0x68,
        0xC3,
        0xA9,
    ]
    assert value_bytes({CONF_DATA: [1, 2], CONF_STRING_ENCODING: "utf_8"}) == [1, 2]


def test_set_value_constants_share_a_flash_table(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("set_value.yaml"))

    assert main_cpp.count("ble_server_value[] PROGMEM = {0x12, 0x34};") == 1
    assert main_cpp.count("set_buffer_static(ble_server_value, 2);") == 2
    assert "set_buffer_template(" in main_cpp
    assert "ByteBuffer::wrap" not in main_cpp


def test_descriptor_set_value_rejects_empty_constant() -> None:
    with pytest.raises(cv.Invalid, match="must not be empty"):
        validate_descriptor_value_not_empty({CONF_VALUE: {CONF_DATA: ""}})
    config = {CONF_VALUE: {CONF_DATA: "x"}}
    assert validate_descriptor_value_not_empty(config) is config


def test_float32_rejects_out_of_range() -> None:
    for value in (1e40, -1e40):
        with pytest.raises(cv.Invalid, match="out of range for a float"):
            float32(value)
    assert float32(3.4028235e38) == 3.4028235e38
    assert float32(float("inf")) == float("inf")
