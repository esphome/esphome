"""Tests for the Bluedroid sscanf caller wraps."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

_WRAP_FLAGS = {
    "-Wl,--wrap=btc_config_get_bin",
    "-Wl,--undefined=__wrap_btc_config_get_bin",
    "-Wl,--wrap=string_to_bdaddr",
    "-Wl,--undefined=__wrap_string_to_bdaddr",
}


@pytest.mark.parametrize(
    ("variant", "framework", "expected"),
    [
        pytest.param("esp32", "{type: esp-idf}", True, id="esp32"),
        pytest.param("esp32c3", "{type: esp-idf}", True, id="c3"),
        pytest.param("esp32s3", "{type: esp-idf}", True, id="s3"),
        pytest.param("esp32c2", "{type: esp-idf}", True, id="c2"),
        pytest.param("esp32c6", "{type: esp-idf}", False, id="c6_rom_sscanf"),
        pytest.param(
            "esp32", "{type: esp-idf, version: 6.0.1}", False, id="idf6_picolibc"
        ),
        pytest.param("esp32", "{type: arduino}", False, id="arduino"),
    ],
)
def test_wraps_only_on_newlib_idf_without_rom_sscanf(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    variant: str,
    framework: str,
    expected: bool,
) -> None:
    """The wraps are emitted only where Bluedroid's sscanf calls link newlib's engine."""
    config = tmp_path / "test.yaml"
    config.write_text(
        f"esphome:\n  name: test\n\nesp32:\n  variant: {variant}\n"
        f"  framework: {framework}\n\nesp32_ble: {{}}\n"
    )
    generate_main(config)
    assert CORE.build_flags & _WRAP_FLAGS == (_WRAP_FLAGS if expected else set())
    defines = {define.name for define in CORE.defines}
    assert ("USE_ESP32_BLE_BLUEDROID_HEX_WRAPS" in defines) is expected
