"""Tests for bthome hub configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.bthome import _validate_unique_macs
from esphome.const import CONF_MAC_ADDRESS


def _devices(*macs: str) -> list[dict]:
    return [{CONF_MAC_ADDRESS: cv.mac_address(mac)} for mac in macs]


def test_distinct_mac_addresses_are_accepted() -> None:
    devices = _devices("11:22:33:44:55:66", "AA:BB:CC:DD:EE:FF")
    assert _validate_unique_macs(devices) == devices


def test_duplicate_mac_address_is_rejected() -> None:
    with pytest.raises(
        cv.Invalid, match="Duplicate remote device MAC address: 11:22:33:44:55:66"
    ):
        _validate_unique_macs(
            _devices("11:22:33:44:55:66", "aa:bb:cc:dd:ee:ff", "11:22:33:44:55:66")
        )


def test_raw_data_trigger_requires_raw_type() -> None:
    from esphome.components.bthome.text_sensor import (
        CONF_ON_RAW_DATA,
        _validate_raw_data_trigger,
    )
    from esphome.const import CONF_TYPE

    raw = {CONF_TYPE: "RAW", CONF_ON_RAW_DATA: []}
    assert _validate_raw_data_trigger(raw) == raw
    with pytest.raises(cv.Invalid, match="only available with type RAW"):
        _validate_raw_data_trigger({CONF_TYPE: "TEXT", CONF_ON_RAW_DATA: []})
