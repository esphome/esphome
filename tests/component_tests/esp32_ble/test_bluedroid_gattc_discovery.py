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
_CLIENTS = {
    "ble_client": (
        "esp32_ble_tracker: {}\n\n"
        "ble_client:\n  - mac_address: AA:BB:CC:DD:EE:FF\n    id: client\n"
    ),
    # The proxy connects through bluetooth_connection, without esp32_ble_client
    "bluetooth_proxy": (
        "wifi:\n  ssid: test\n  password: password1\n\napi: {}\n\n"
        "bluetooth_proxy:\n  active: true\n"
    ),
    "none": "",
}


@pytest.mark.parametrize(
    ("framework", "client", "expected"),
    [
        # Every ESP-IDF drops the status, not only the releases with the strict handle check
        pytest.param(
            "{type: esp-idf, version: 5.5.5}", "ble_client", True, id="idf_5_5_5"
        ),
        pytest.param(
            "{type: esp-idf, version: 5.5.6}", "ble_client", True, id="idf_5_5_6"
        ),
        pytest.param(
            "{type: esp-idf, version: 6.0.1}", "ble_client", True, id="idf_6_0_1"
        ),
        pytest.param("{type: arduino}", "ble_client", True, id="arduino"),
        pytest.param(
            "{type: esp-idf}\n  toolchain: platformio",
            "ble_client",
            True,
            id="platformio",
        ),
        pytest.param("{type: esp-idf}", "none", False, id="no_client"),
        pytest.param("{type: esp-idf}", "bluetooth_proxy", True, id="proxy"),
    ],
)
def test_wrap_only_with_a_gatt_client(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    framework: str,
    client: str,
    expected: bool,
) -> None:
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\nesp32:\n  variant: esp32\n"
        f"  framework: {framework}\n\nesp32_ble: {{}}\n\n" + _CLIENTS[client]
    )
    generate_main(config)
    assert CORE.build_flags & _WRAP_FLAGS == (_WRAP_FLAGS if expected else set())
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_GATTC_DISCOVERY_STATUS" in defines) is expected
