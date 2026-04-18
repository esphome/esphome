"""Tests for modbus configuration validation."""

from esphome import config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_modbus_accepts_rx_buffer_delay(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.modbus import CONF_RX_BUFFER_DELAY, CONFIG_SCHEMA

    config = CONFIG_SCHEMA(
        {
            "id": "modbus_bus",
            "uart_id": "uart_bus",
            CONF_RX_BUFFER_DELAY: "25ms",
        }
    )

    assert config[CONF_RX_BUFFER_DELAY] == cv.positive_time_period_milliseconds("25ms")
