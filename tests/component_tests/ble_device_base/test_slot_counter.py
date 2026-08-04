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


def _define_value(name: str) -> str | None:
    for define in CORE.defines:
        if define.name == name:
            # Values are codegen expressions (IntLiteral); compare rendered.
            return str(define.value)
    return None


@pytest.mark.parametrize(
    ("config", "define"),
    [("bk72xx_tracker.yaml", "BK72XX_BLE_SCAN_LISTENER_COUNT")],
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
    assert _define_value(define) == "1"
    assert _define_value("ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT") is None


@pytest.mark.parametrize(
    ("config", "define"),
    [("bk72xx_controller_only.yaml", "BK72XX_BLE_SCAN_LISTENER_COUNT")],
)
def test_controller_only_emits_no_count(
    config: str,
    define: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """No consumer, no define — the guarded listener storage compiles out."""
    generate_main(component_config_path(config))
    assert _define_value(define) is None
