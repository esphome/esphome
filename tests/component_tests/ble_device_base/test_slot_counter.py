"""Tests for the shared slot_counter codegen factory.

The factory is exercised end to end through the real controllers: a tracker
config must emit the platform's scan listener count define, and a
controller-only config must emit nothing so the guarded StaticVector storage
compiles out.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

from ..helpers import get_define_value


def test_tracker_requests_one_slot(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The tracker's to_code requests a slot; the FINAL job emits the count.

    The neutral listener count must stay absent from the same build: no BLE
    consumer registered through register_ble_device().
    """
    generate_main(component_config_path("bk72xx_tracker.yaml"))
    assert get_define_value("BK72XX_BLE_SCAN_LISTENER_COUNT") == "1"
    assert get_define_value("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT") is None


def test_controller_only_emits_no_count(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """No consumer, no define — the guarded listener storage compiles out."""
    generate_main(component_config_path("bk72xx_controller_only.yaml"))
    assert get_define_value("BK72XX_BLE_SCAN_LISTENER_COUNT") is None


def test_neutral_listener_count_emitted_when_requested() -> None:
    """The neutral counter is wired to its define: one request through the
    slot bound in register_ble_device() emits the count.

    No in-tree sensor registers through ble_device_base.register_ble_device()
    yet (consumer migration is a follow-up), so the request function is driven
    directly; every tracker's #ifdef-guarded listener storage keys on this
    define, and a broken emit path would compile the storage out silently.
    """
    from esphome.components import ble_device_base

    ble_device_base._request_listener_slot()
    CORE.flush_tasks()
    assert get_define_value(ble_device_base.LISTENER_COUNT_DEFINE) == "1"
