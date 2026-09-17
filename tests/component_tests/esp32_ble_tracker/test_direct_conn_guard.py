"""Tests for the Bluedroid queued connection guard.

Builds that open BLE connections wrap l2cble_init_direct_conn, scan-only
builds emit nothing.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

# Spelled out rather than derived from the component, so a typo in the
# component's flags fails here instead of mirroring into the test.
_GUARD_FLAGS = {
    "-Wl,--wrap=l2cble_init_direct_conn",
    "-Wl,--undefined=__wrap_l2cble_init_direct_conn",
}


@pytest.mark.parametrize(
    ("config_file", "expected"),
    [
        pytest.param("scan_window_raised.yaml", True, id="client"),
        pytest.param("scan_window_scan_only.yaml", False, id="scan_only"),
    ],
)
def test_guard_only_in_builds_with_a_client(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    expected: bool,
) -> None:
    generate_main(component_config_path(config_file))
    assert (CORE.build_flags >= _GUARD_FLAGS) is expected
    assert CORE.build_flags.isdisjoint(_GUARD_FLAGS) is not expected
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_TRACKER_DIRECT_CONN_GUARD" in defines) is expected
