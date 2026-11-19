"""Tests for the wrap that sorts GATT discovery responses by handle."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32_ble import _has_strict_handle_order
import esphome.config_validation as cv
from esphome.core import CORE

_WRAP_FLAGS = {
    "-Wl,--wrap=gatt_client_handle_server_rsp",
    "-Wl,--undefined=__wrap_gatt_client_handle_server_rsp",
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
    ("version", "expected"),
    [
        ("5.4.4", False),
        ("5.5.5", False),
        ("5.5.6", True),
        ("5.5.7", True),
        ("6.0.0", False),
        ("6.0.2", False),
        ("6.0.3", True),
        ("6.1.0", True),
    ],
)
def test_strict_handle_order_versions(version: str, expected: bool) -> None:
    """ESP-IDF added the check in 5.5.6 and 6.0.3."""
    assert _has_strict_handle_order(cv.Version.parse(version)) is expected


@pytest.mark.parametrize(
    ("framework", "client", "expected"),
    [
        pytest.param(
            "{type: esp-idf, version: 5.5.6}", "ble_client", True, id="idf_5_5_6"
        ),
        pytest.param(
            "{type: esp-idf, version: 5.5.6}", "bluetooth_proxy", True, id="proxy"
        ),
        pytest.param(
            "{type: esp-idf, version: 5.5.5}", "ble_client", False, id="idf_5_5_5"
        ),
        pytest.param(
            "{type: esp-idf, version: 6.0.1}", "ble_client", False, id="idf_6_0_1"
        ),
        pytest.param("{type: esp-idf, version: 5.5.6}", "none", False, id="no_client"),
        # Arduino 3.3.12 is built on ESP-IDF 5.5.6; no private headers are needed
        pytest.param(
            "{type: arduino, version: 3.3.12}", "ble_client", True, id="arduino"
        ),
        pytest.param(
            "{type: esp-idf, version: 5.5.6}\n  toolchain: platformio",
            "ble_client",
            True,
            id="platformio",
        ),
    ],
)
def test_wrap_only_with_a_gatt_client_on_strict_idf(
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
    assert ("USE_ESP32_BLE_GATTC_HANDLE_ORDER" in defines) is expected
