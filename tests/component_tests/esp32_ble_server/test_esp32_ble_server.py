"""Tests for esp32_ble_server configuration helpers."""

import pytest

from esphome.components.esp32_ble_server import (
    CCCD_DESCRIPTOR_UUID,
    CUD_DESCRIPTOR_UUID,
    DEVICE_INFORMATION_SERVICE_UUID,
    uuid_is,
)


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
