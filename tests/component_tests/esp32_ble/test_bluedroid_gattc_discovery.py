"""Tests for the wrap that keeps the status of a failed GATT discovery."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

_WRAP_FLAGS = {
    "-Wl,--wrap=bta_gattc_disc_cmpl_cback",
    "-Wl,--undefined=__wrap_bta_gattc_disc_cmpl_cback",
}
_CLIENT = (
    "esp32_ble_tracker: {}\n\n"
    "ble_client:\n  - mac_address: AA:BB:CC:DD:EE:FF\n    id: client\n"
)


@pytest.mark.parametrize(
    ("framework", "client", "expected"),
    [
        # Every ESP-IDF drops the status, not only the releases with the strict handle check
        pytest.param("{type: esp-idf, version: 5.5.5}", True, True, id="idf_5_5_5"),
        pytest.param("{type: esp-idf, version: 5.5.6}", True, True, id="idf_5_5_6"),
        pytest.param("{type: esp-idf, version: 6.0.1}", True, True, id="idf_6_0_1"),
        pytest.param("{type: arduino}", True, True, id="arduino"),
        pytest.param(
            "{type: esp-idf}\n  toolchain: platformio", True, True, id="platformio"
        ),
        pytest.param("{type: esp-idf}", False, False, id="no_client"),
    ],
)
def test_wrap_only_with_a_gatt_client(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    framework: str,
    client: bool,
    expected: bool,
) -> None:
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\nesp32:\n  variant: esp32\n"
        f"  framework: {framework}\n\nesp32_ble: {{}}\n\n" + (_CLIENT if client else "")
    )
    generate_main(config)
    assert CORE.build_flags & _WRAP_FLAGS == (_WRAP_FLAGS if expected else set())
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_GATTC_DISCOVERY_STATUS" in defines) is expected
