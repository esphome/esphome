"""Tests for the shared slot_counter codegen factory.

The factory is exercised end to end through the real controllers: a tracker
config must emit the platform's scan listener count define, and a
controller-only config must emit nothing so the guarded StaticVector storage
compiles out.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

from ..helpers import get_define_value


@pytest.mark.parametrize(
    ("config", "define"),
    [
        ("bk72xx_tracker.yaml", "BK72XX_BLE_SCAN_LISTENER_COUNT"),
        ("rp2_tracker.yaml", "RP2040_BLE_SCAN_LISTENER_COUNT"),
    ],
)
def test_tracker_requests_one_slot(
    config: str,
    define: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The tracker's to_code requests a slot; the FINAL job emits the count.

    The neutral listener count must stay absent from the same build: no BLE
    consumer registered through register_ble_device().
    """
    generate_main(component_config_path(config))
    assert get_define_value(define) == "1"
    assert get_define_value("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT") is None


@pytest.mark.parametrize(
    ("config", "define"),
    [
        ("bk72xx_controller_only.yaml", "BK72XX_BLE_SCAN_LISTENER_COUNT"),
        ("rp2_controller_only.yaml", "RP2040_BLE_SCAN_LISTENER_COUNT"),
    ],
)
def test_controller_only_emits_no_count(
    config: str,
    define: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """No consumer, no define — the guarded listener storage compiles out."""
    generate_main(component_config_path(config))
    assert get_define_value(define) is None


def test_neutral_listener_count_emitted_when_requested() -> None:
    """Registering through register_ble_device() emits the neutral count.

    No in-tree sensor registers through ble_device_base.register_ble_device()
    yet (consumer migration is a follow-up), so the coroutine is driven with a
    mock hub instead of a config; every tracker's #ifdef-guarded listener
    storage keys on this define, and a broken emit path would compile the
    storage out silently.
    """
    import esphome.codegen as cg
    from esphome.components import ble_device_base
    from esphome.core import ID

    hub_id = ID("hub", type=ble_device_base.BLEHub)
    CORE.register_variable(hub_id, cg.MockObj("hub"))
    CORE.add_job(
        ble_device_base.register_ble_device,
        cg.MockObj("listener"),
        {ble_device_base.CONF_BLE_HUB_ID: hub_id},
    )
    CORE.flush_tasks()
    assert get_define_value(ble_device_base.LISTENER_COUNT_DEFINE) == "1"


def test_esp32_tracker_handler_counts(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A bare tracker registers its four esp32_ble handlers and nothing else."""
    generate_main(component_config_path("esp32_tracker_only.yaml"))
    assert get_define_value("ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT") == "1"
    assert get_define_value("ESPHOME_ESP32_BLE_GAP_SCAN_EVENT_HANDLER_COUNT") == "1"
    assert get_define_value("ESPHOME_ESP32_BLE_GATTC_EVENT_HANDLER_COUNT") == "1"
    assert get_define_value("ESPHOME_ESP32_BLE_BLE_STATUS_EVENT_HANDLER_COUNT") == "1"
    assert get_define_value("ESPHOME_ESP32_BLE_GATTS_EVENT_HANDLER_COUNT") is None
    # No advertisement listener or client is registered, so both storages compile out.
    assert get_define_value("ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT") is None
    assert get_define_value("ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT") is None


def test_esp32_bluetooth_proxy_requests_client_slots_only(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The proxy requests a client slot per connection (three by default with
    active: true); advertisements and scanner state arrive through the hub
    callbacks, so no listener slot exists."""
    generate_main(component_config_path("esp32_bluetooth_proxy.yaml"))
    assert get_define_value("ESPHOME_ESP32_BLE_TRACKER_LISTENER_COUNT") is None
    assert get_define_value("ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT") == "3"
    # One neutral GATT backend slot per connection (the hub-model flip).
    assert get_define_value("ESPHOME_BLE_GATT_CLIENT_COUNT") == "3"


def test_counts_reset_between_compiles(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A second compile in the same process starts from zero.

    The module level counters this change removes leaked across compiles in a
    long lived host process (dashboard, device-builder), growing the handler
    counts by one per compile and oversizing the StaticCallbackManager storage.
    """
    generate_main(component_config_path("esp32_tracker_only.yaml"))
    assert get_define_value("ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT") == "1"
    CORE.reset()
    generate_main(component_config_path("esp32_tracker_only.yaml"))
    assert get_define_value("ESPHOME_ESP32_BLE_GAP_EVENT_HANDLER_COUNT") == "1"
