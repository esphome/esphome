"""Tests for the cross-component GATT slot ledger."""

import pytest

from esphome import config_validation as cv
from esphome.components import (
    ble_client,
    ble_device_base,
    bluetooth_connection,
    bluetooth_proxy,
)
from esphome.const import (
    CONF_MAC_ADDRESS,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_RP2,
    PlatformFramework,
)
from esphome.core import CORE

from ..types import SetCoreConfigCallable


def test_gatt_slot_ledger_rejects_overcommit_on_rp2() -> None:
    # The cap logic in isolation: two hand charges must trip it.
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_RP2
    bluetooth_connection.consume_gatt_slot("bluetooth_proxy")({})
    bluetooth_connection.consume_gatt_slot("ble_client")({})
    with pytest.raises(cv.Invalid, match="supports at most 1 GATT client"):
        bluetooth_connection.FINAL_VALIDATE_SCHEMA({})


def test_gatt_slot_ledger_skipped_in_testing_mode() -> None:
    # Grouped component builds merge fixtures past the cap; the check defers
    # to testing mode like esp32_ble.validate_connection_slots.
    CORE.data.setdefault(KEY_CORE, {})[KEY_TARGET_PLATFORM] = PLATFORM_RP2
    bluetooth_connection.consume_gatt_slot("bluetooth_proxy")({})
    bluetooth_connection.consume_gatt_slot("ble_client")({})
    CORE.testing_mode = True
    try:
        bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
    finally:
        CORE.testing_mode = False


def test_real_validators_charge_the_ledger_on_rp2(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # End to end through the component CONFIG_SCHEMAs (no hand charges):
    # removing either consumer's consume_gatt_slot call fails this test.
    set_core_config(PlatformFramework.RP2_ARDUINO)
    ble_device_base.register_hub_provider("rp2_ble_tracker")
    CORE.loaded_integrations.add("rp2_ble_tracker")
    bluetooth_proxy.CONFIG_SCHEMA({})
    ble_client.CONFIG_SCHEMA({CONF_MAC_ADDRESS: "AA:BB:CC:DD:EE:FF"})
    with pytest.raises(cv.Invalid, match="requested by: bluetooth_proxy, ble_client"):
        bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
