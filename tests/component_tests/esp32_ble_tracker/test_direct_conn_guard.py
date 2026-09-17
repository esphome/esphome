"""The Bluedroid queued connection guard: builds that open BLE connections
wrap l2cble_init_direct_conn, scan-only builds emit nothing."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

# Spelled out rather than derived from the component, so a typo in the
# component's flags fails here instead of mirroring into the test.
GUARD_FLAGS = (
    "-Wl,--wrap=l2cble_init_direct_conn",
    "-Wl,--undefined=__wrap_l2cble_init_direct_conn",
)
GUARD_DEFINE = "USE_ESP32_BLE_DIRECT_CONN_GUARD"


def _define_names() -> set[str]:
    return {define.name for define in CORE.defines}


def test_client_build_emits_the_guard(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("scan_window_raised.yaml"))
    assert all(flag in CORE.build_flags for flag in GUARD_FLAGS)
    assert GUARD_DEFINE in _define_names()


def test_scan_only_build_has_no_guard(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("scan_window_scan_only.yaml"))
    assert not any(flag in CORE.build_flags for flag in GUARD_FLAGS)
    assert GUARD_DEFINE not in _define_names()
