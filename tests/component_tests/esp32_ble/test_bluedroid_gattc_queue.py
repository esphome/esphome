"""Tests for the Bluedroid wraps that drop a failed queued GATT client command."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32.const import KEY_ESP32, KEY_SRC_CMAKE
from esphome.components.esp32_ble import bluedroid_headers_cmake
from esphome.core import CORE

_WRAPPED = (
    "bta_gattc_cfg_mtu",
    "bta_gattc_execute",
    "GATTC_ConfigureMTU",
    "GATTC_ExecuteWrite",
)
_WRAP_FLAGS = {f"-Wl,--wrap={s}" for s in _WRAPPED} | {
    f"-Wl,--undefined=__wrap_{s}" for s in _WRAPPED
}
_TRACKER = "\nesp32_ble_tracker: {}\n"


@pytest.mark.parametrize(
    ("framework", "components", "windows", "expected"),
    [
        pytest.param(
            "{type: esp-idf, version: 5.5.6}", _TRACKER, False, True, id="idf_5_5_6"
        ),
        pytest.param(
            "{type: esp-idf, version: 6.0.1}", _TRACKER, False, True, id="idf_6"
        ),
        pytest.param(
            "{type: arduino, version: 3.3.12}", _TRACKER, False, True, id="arduino"
        ),
        # Without the tracker or esp32_ble_client there is no GATT client
        pytest.param(
            "{type: esp-idf, version: 5.5.6}", "", False, False, id="no_gatt_client"
        ),
        pytest.param(
            "{type: esp-idf, version: 5.5.6}", _TRACKER, True, False, id="windows"
        ),
        # PlatformIO builds cannot reach the private headers
        pytest.param(
            "{type: esp-idf, version: 5.5.6}\n  toolchain: platformio",
            _TRACKER,
            False,
            False,
            id="platformio",
        ),
    ],
)
def test_wraps_on_native_builds_with_a_gatt_client(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    framework: str,
    components: str,
    windows: bool,
    expected: bool,
) -> None:
    monkeypatch.setattr("esphome.components.esp32_ble.IS_WINDOWS", windows)
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\nesp32:\n  variant: esp32\n"
        f"  framework: {framework}\n\nesp32_ble: {{}}\n{components}"
    )
    generate_main(config)
    assert CORE.build_flags & _WRAP_FLAGS == (_WRAP_FLAGS if expected else set())
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_BLUEDROID_GATTC_QUEUE" in defines) is expected
    headers = bluedroid_headers_cmake("bluedroid_gattc_queue.c")
    assert (headers in CORE.data[KEY_ESP32][KEY_SRC_CMAKE]) is expected
