import logging

import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIMETER,
)
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@TH-Braemer"]
DEPENDENCIES = ["uart"]

a02yyuw_ns = cg.esphome_ns.namespace("a02yyuw")
A02yyuwComponent = a02yyuw_ns.class_(
    "A02yyuwComponent", sensor.Sensor, cg.Component, uart.UARTDevice
)


def _warn_deprecated(config: ConfigType) -> ConfigType:
    _LOGGER.warning(
        "The 'a02yyuw' component is deprecated and will be removed in a future release. "
        "Please migrate to the 'distance_uart' component (note: 'distance_uart' publishes in "
        "meters instead of millimeters and defaults to 'publish_mode: INTERVAL')."
    )
    return config


CONFIG_SCHEMA = sensor.sensor_schema(
    A02yyuwComponent,
    unit_of_measurement=UNIT_MILLIMETER,
    icon=ICON_ARROW_EXPAND_VERTICAL,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_DISTANCE,
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        "a02yyuw",
        baud_rate=9600,
        require_tx=False,
        require_rx=True,
        data_bits=8,
        parity=None,
        stop_bits=1,
    ),
    _warn_deprecated,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
