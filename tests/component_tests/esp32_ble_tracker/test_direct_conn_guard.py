"""Tests for the Bluedroid queued connection guard.

Builds that open BLE connections on an ESP-IDF release without the upstream
fix wrap l2cble_init_direct_conn; scan-only builds and fixed releases emit
nothing.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.core import CORE

# Spelled out rather than derived from the component, so a typo in the
# component's flags fails here instead of mirroring into the test.
_GUARD_FLAGS = {
    "-Wl,--wrap=l2cble_init_direct_conn",
    "-Wl,--undefined=__wrap_l2cble_init_direct_conn",
}


def _pin_idf(monkeypatch: pytest.MonkeyPatch, idf: str) -> None:
    monkeypatch.setattr(esp32_ble_tracker, "idf_version", lambda: cv.Version.parse(idf))


@pytest.mark.parametrize(
    ("config_file", "idf", "expected"),
    [
        pytest.param("scan_window_raised.yaml", "5.5.5", True, id="client"),
        pytest.param("scan_window_raised.yaml", "6.0.3", False, id="client_fixed_idf"),
        pytest.param("scan_window_scan_only.yaml", "5.5.5", False, id="scan_only"),
    ],
)
def test_guard_only_in_client_builds_on_unfixed_idf(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    monkeypatch: pytest.MonkeyPatch,
    config_file: str,
    idf: str,
    expected: bool,
) -> None:
    # Pinned so the test does not follow the default framework version.
    _pin_idf(monkeypatch, idf)
    generate_main(component_config_path(config_file))
    assert (CORE.build_flags >= _GUARD_FLAGS) is expected
    assert CORE.build_flags.isdisjoint(_GUARD_FLAGS) is not expected
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD" in defines) is expected


@pytest.mark.parametrize(
    ("idf", "expected"),
    [
        ("5.1.6", True),  # series that never got the fix
        ("5.2.7", True),
        ("5.2.8", False),
        ("5.3.5", True),
        ("5.3.6", False),
        ("5.4.4", True),
        ("5.4.5", False),
        ("5.5.5", True),
        ("5.5.6", False),
        ("6.0.2", True),
        ("6.0.3", False),
        ("6.1.0", True),
        ("6.1.1", False),
        ("6.2.0", False),
        ("7.0.0", False),
    ],
)
def test_needs_direct_conn_guard(
    monkeypatch: pytest.MonkeyPatch, idf: str, expected: bool
) -> None:
    _pin_idf(monkeypatch, idf)
    assert esp32_ble_tracker._needs_direct_conn_guard() is expected
