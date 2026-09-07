"""Tests for the cross-component GATT slot ledger."""

import pytest

from esphome import config_validation as cv
from esphome.components import (
    ble_client,
    ble_device_base,
    bluetooth_connection,
    bluetooth_proxy,
    rp2040_ble,
)
from esphome.const import CONF_MAC_ADDRESS, PlatformFramework
from esphome.core import CORE

from ..types import SetCoreConfigCallable


def test_gatt_slot_ledger_rejects_overcommit_on_rp2(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # rp2 owns its budget: the stack's validation reports the overcommit and
    # the neutral cap check stays silent (one message per misconfiguration).
    set_core_config(PlatformFramework.RP2_ARDUINO)
    bluetooth_connection.consume_gatt_slot("bluetooth_proxy", 3)({})
    bluetooth_connection.consume_gatt_slot("ble_client")({})
    bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
    with pytest.raises(cv.Invalid, match="rp2 maximum is 3"):
        rp2040_ble.validate_connection_slots()


def test_gatt_slot_ledger_skipped_in_testing_mode(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # Grouped component builds merge fixtures past the cap; the check defers
    # to testing mode like esp32_ble.validate_connection_slots.
    set_core_config(PlatformFramework.RP2_ARDUINO)
    bluetooth_connection.consume_gatt_slot("bluetooth_proxy", 3)({})
    bluetooth_connection.consume_gatt_slot("ble_client")({})
    CORE.testing_mode = True
    try:
        bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
        rp2040_ble.validate_connection_slots()
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
    # The proxy defaults to 3 slots on rp2; ble_client's claim overcommits
    # and rp2's own budget names every claimant.
    with pytest.raises(
        cv.Invalid,
        match="Components: bluetooth_proxy, bluetooth_proxy, bluetooth_proxy, "
        "ble_client",
    ):
        rp2040_ble.validate_connection_slots()


def test_neutral_cap_check_guards_future_hub_platforms(
    set_core_config: SetCoreConfigCallable, monkeypatch: pytest.MonkeyPatch
) -> None:
    # Both current platforms defer to their stack budgets; pin the message
    # and boundary of the branch a future budget-less hub platform takes.
    set_core_config(PlatformFramework.RP2_ARDUINO)
    monkeypatch.setattr(bluetooth_connection, "_STACK_BUDGET_PLATFORMS", set())
    bluetooth_connection.consume_gatt_slot("bluetooth_proxy", 3)({})
    bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
    bluetooth_connection.consume_gatt_slot("ble_client")({})
    with pytest.raises(cv.Invalid, match="supports at most 3 GATT client connection"):
        bluetooth_connection.FINAL_VALIDATE_SCHEMA({})
