"""Tests for it8951 configuration validation."""

from collections.abc import Callable
from typing import Any

from esphome.components.const import CONF_HOLD_STATE
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32S3
from esphome.components.it8951.display import CONFIG_SCHEMA
from esphome.const import CONF_ENABLE_PIN, CONF_NUMBER, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_reterminal_e1003_enable_pins_hold_state(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The reTerminal E1003 enable pins must hold their state during deep sleep."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 7, "mosi_pin": 9})

    config = CONFIG_SCHEMA({"id": "test_display", "model": "seeed-reterminal-e1003"})

    enable_pins = config[CONF_ENABLE_PIN]
    assert [pin[CONF_NUMBER] for pin in enable_pins] == [21, 11]
    assert all(pin[CONF_HOLD_STATE] for pin in enable_pins)
