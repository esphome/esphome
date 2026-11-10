import logging

import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@MrSuicideParrot"]
DEPENDENCIES = ["uart"]

a01nyub_ns = cg.esphome_ns.namespace("a01nyub")
A01nyubComponent = a01nyub_ns.class_(
    "A01nyubComponent", sensor.Sensor, cg.Component, uart.UARTDevice
)


def _warn_deprecated(config: ConfigType) -> ConfigType:
    _LOGGER.warning(
        "The 'a01nyub' component is deprecated and will be removed in a future release. "
        "Please migrate to the 'distance_uart' component (note: 'distance_uart' defaults to "
        "'publish_mode: INTERVAL' instead of publishing every frame)."
    )
    return config


CONFIG_SCHEMA = sensor.sensor_schema(
    A01nyubComponent,
    unit_of_measurement=UNIT_METER,
    icon=ICON_ARROW_EXPAND_VERTICAL,
    accuracy_decimals=3,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_DISTANCE,
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = cv.All(
    uart.final_validate_device_schema(
        "a01nyub",
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
