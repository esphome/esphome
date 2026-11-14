"""Tests for the Bluedroid wraps that keep bond keys on re-encryption."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_SRC_CMAKE
from esphome.components.esp32_ble import bluedroid_headers_cmake
from esphome.core import CORE

_WRAP_FLAGS = {
    "-Wl,--wrap=smp_check_auth_req",
    "-Wl,--undefined=__wrap_smp_check_auth_req",
    "-Wl,--wrap=btm_sec_clear_ble_keys",
    "-Wl,--undefined=__wrap_btm_sec_clear_ble_keys",
}


@pytest.mark.parametrize(
    ("framework", "windows", "expected"),
    [
        pytest.param("{type: esp-idf, version: 5.5.6}", False, True, id="idf_5_5_6"),
        pytest.param("{type: esp-idf, version: 5.5.5}", False, False, id="idf_5_5_5"),
        pytest.param("{type: esp-idf, version: 6.0.1}", False, False, id="idf_6"),
        pytest.param("{type: esp-idf, version: 5.5.6}", True, False, id="windows"),
        # Arduino 3.3.12 is built on ESP-IDF 5.5.6, 3.3.11 on 5.5.5
        pytest.param(
            "{type: arduino, version: 3.3.12}", False, True, id="arduino_3_3_12"
        ),
        pytest.param(
            "{type: arduino, version: 3.3.11}", False, False, id="arduino_3_3_11"
        ),
        # PlatformIO builds cannot reach the private headers
        pytest.param(
            "{type: esp-idf, version: 5.5.6}\n  toolchain: platformio",
            False,
            False,
            id="platformio_idf_5_5_6",
        ),
        pytest.param(
            "{type: arduino, version: 3.3.12}\n  toolchain: platformio",
            False,
            False,
            id="platformio_arduino_3_3_12",
        ),
    ],
)
def test_wraps_only_on_idf_5_5_6_native_builds(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    framework: str,
    windows: bool,
    expected: bool,
) -> None:
    monkeypatch.setattr("esphome.components.esp32_ble.IS_WINDOWS", windows)
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\nesp32:\n  variant: esp32\n"
        f"  framework: {framework}\n\nesp32_ble: {{}}\n"
    )
    generate_main(config)
    assert CORE.build_flags & _WRAP_FLAGS == (_WRAP_FLAGS if expected else set())
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_BLUEDROID_BOND_KEYS" in defines) is expected
    headers = bluedroid_headers_cmake("bluedroid_bond_keys.c")
    assert (headers in CORE.data[KEY_ESP32][KEY_SRC_CMAKE]) is expected
