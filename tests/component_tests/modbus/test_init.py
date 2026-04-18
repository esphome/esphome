"""Tests for modbus configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_modbus_accepts_flow_control_delays(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={"board": "esp32dev", "variant": "ESP32"},
    )
    from esphome.components import esp32  # noqa: F401
    from esphome.components.modbus import (
        CONF_FLOW_CONTROL_PIN_POST_SEND_DELAY,
        CONF_FLOW_CONTROL_PIN_PRE_SEND_DELAY,
        CONFIG_SCHEMA,
    )

    config = CONFIG_SCHEMA(
        {
            "id": "modbus_bus",
            "uart_id": "uart_bus",
            "flow_control_pin": 4,
            CONF_FLOW_CONTROL_PIN_PRE_SEND_DELAY: "5ms",
            CONF_FLOW_CONTROL_PIN_POST_SEND_DELAY: "7ms",
        }
    )

    assert config[
        CONF_FLOW_CONTROL_PIN_PRE_SEND_DELAY
    ] == cv.positive_time_period_milliseconds("5ms")
    assert config[
        CONF_FLOW_CONTROL_PIN_POST_SEND_DELAY
    ] == cv.positive_time_period_milliseconds("7ms")


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {
                "id": "modbus_bus",
                "uart_id": "uart_bus",
                "flow_control_pin_pre_send_delay": "5ms",
            },
            "'flow_control_pin_pre_send_delay' requires 'flow_control_pin'",
            id="pre_send_delay_requires_flow_control_pin",
        ),
        pytest.param(
            {
                "id": "modbus_bus",
                "uart_id": "uart_bus",
                "flow_control_pin_post_send_delay": "7ms",
            },
            "'flow_control_pin_post_send_delay' requires 'flow_control_pin'",
            id="post_send_delay_requires_flow_control_pin",
        ),
    ],
)
def test_modbus_rejects_flow_control_delays_without_flow_control_pin(
    config: dict[str, str],
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.modbus import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=error_match):
        CONFIG_SCHEMA(config)
