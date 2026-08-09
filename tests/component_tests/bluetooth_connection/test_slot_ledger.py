"""Tests for the cross-component GATT slot ledger."""

import pytest

from esphome import config_validation as cv
from esphome.components import bluetooth_connection
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM, PLATFORM_RP2
from esphome.core import CORE


def test_gatt_slot_ledger_rejects_overcommit_on_rp2() -> None:
    # The cross-component cap must reject two claims on rp2.
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_RP2
    bluetooth_connection.consume_gatt_slot("bluetooth_proxy")({})
    bluetooth_connection.consume_gatt_slot("ble_client")({})
    with pytest.raises(cv.Invalid, match="supports at most 1 GATT client"):
        bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
