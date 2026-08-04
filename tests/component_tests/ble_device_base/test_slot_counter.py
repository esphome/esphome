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


def _define_value(name: str) -> str | None:
    for define in CORE.defines:
        if define.name == name:
            # Values are codegen expressions (IntLiteral); compare rendered.
            return str(define.value)
    return None


def test_bk72xx_tracker_requests_one_slot(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The tracker's to_code requests a slot; the FINAL job emits the count."""
    generate_main(component_config_path("bk72xx_tracker.yaml"))
    assert _define_value("BK72XX_BLE_SCAN_LISTENER_COUNT") == "1"


def test_bk72xx_controller_only_emits_no_count(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """No consumer, no define — the guarded listener storage compiles out."""
    generate_main(component_config_path("bk72xx_controller_only.yaml"))
    assert _define_value("BK72XX_BLE_SCAN_LISTENER_COUNT") is None


def test_neutral_listener_count_absent_without_consumers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """ble_device_base's centralized job emits nothing while no BLE consumer
    registers through register_ble_device()."""
    generate_main(component_config_path("bk72xx_tracker.yaml"))
    assert _define_value("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT") is None
