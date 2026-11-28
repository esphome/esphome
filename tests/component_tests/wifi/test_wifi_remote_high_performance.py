"""Tests for the high-performance WiFi settings on an esp32_hosted co-processor.

On a chip with no radio the CONFIG_ESP_WIFI_* buffer options are prompt-less
aliases of esp_wifi_remote's CONFIG_WIFI_RMT_* ones, so only the latter reach
the co-processor.
"""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.network import require_high_performance_wifi
from tests.component_tests.network import sdkconfig_option

_DYNAMIC_TX = "CONFIG_WIFI_RMT_DYNAMIC_TX_BUFFER_NUM"

_C6_VALUES = {
    "CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM": 16,
    "CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM": 64,
    _DYNAMIC_TX: 64,
    "CONFIG_WIFI_RMT_AMPDU_TX_ENABLED": True,
    "CONFIG_WIFI_RMT_TX_BA_WIN": 32,
    "CONFIG_WIFI_RMT_AMPDU_RX_ENABLED": True,
    "CONFIG_WIFI_RMT_RX_BA_WIN": 32,
}

_HOSTED = (
    "esp32_hosted:\n  variant: {variant}\n  reset_pin: GPIO54\n"
    "  cmd_pin: GPIO19\n  clk_pin: GPIO18\n  d0_pin: GPIO14\n  d1_pin: GPIO15\n"
    "  d2_pin: GPIO16\n  d3_pin: GPIO17\n\n"
)

_PSRAM = "psram:\n  ignore_not_found: false\n\n"


def _write(tmp_path: Path, chip: str, hosted_variant: str | None, psram: bool) -> Path:
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\n"
        f"esp32:\n  variant: {chip}\n  framework: {{type: esp-idf}}\n\n"
        + (_HOSTED.format(variant=hosted_variant) if hosted_variant else "")
        + (_PSRAM if psram else "")
        + 'wifi:\n  ssid: "test_ssid"\n  password: "test_password"\n'
    )
    return config


@pytest.mark.parametrize(
    ("psram", "dynamic_tx"),
    [
        pytest.param(False, 64, id="no_psram"),
        # with guaranteed PSRAM only static TX buffers exist, so no dynamic count
        pytest.param(True, None, id="psram"),
    ],
)
def test_c6_coprocessor_gets_the_settings(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    psram: bool,
    dynamic_tx: int | None,
) -> None:
    require_high_performance_wifi()
    generate_main(_write(tmp_path, "esp32p4", "ESP32C6", psram))
    for name, value in (_C6_VALUES | {_DYNAMIC_TX: dynamic_tx}).items():
        assert sdkconfig_option(name) == value, name
    # the native options would be ignored without a radio, so they are not written
    assert sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM") is None


@pytest.mark.parametrize(
    ("chip", "hosted_variant", "requested"),
    [
        pytest.param("esp32p4", "ESP32C6", False, id="not_requested"),
        # values published for an ESP32-C6 are not pushed onto another chip's RAM
        pytest.param("esp32p4", "ESP32C5", True, id="other_coprocessor"),
        pytest.param("esp32s3", None, True, id="native_wifi"),
    ],
)
def test_defaults_are_kept(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    chip: str,
    hosted_variant: str | None,
    requested: bool,
) -> None:
    if requested:
        require_high_performance_wifi()
    generate_main(_write(tmp_path, chip, hosted_variant, False))
    for name in _C6_VALUES:
        assert sdkconfig_option(name) is None, name


def test_c5_host_with_coprocessor_gets_the_settings(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    """With use_coprocessor the host's own radio is idle; the C6 values go to the co-processor."""
    config = tmp_path / "test.yaml"
    config.write_text(
        "esphome:\n  name: test\n\n"
        "esp32:\n  variant: esp32c5\n  framework: {type: esp-idf}\n\n"
        "esp32_hosted:\n  type: spi\n  variant: ESP32C6\n  reset_pin: GPIO25\n"
        "  handshake_pin: GPIO1\n  data_ready_pin: GPIO0\n  miso_pin: GPIO9\n"
        "  mosi_pin: GPIO10\n  clk_pin: GPIO8\n  cs_pin: GPIO7\n\n"
        "wifi:\n  use_coprocessor: true\n"
        '  ssid: "test_ssid"\n  password: "test_password"\n'
    )
    require_high_performance_wifi()
    generate_main(config)
    for name, value in _C6_VALUES.items():
        assert sdkconfig_option(name) == value, name
    assert sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM") is None
