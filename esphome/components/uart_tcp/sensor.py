import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_TOTAL_INCREASING,
)
from esphome.types import ConfigType

from . import CONF_UART_TCP_ID, UartTcp

DEPENDENCIES = ["uart_tcp"]

CONF_DISCONNECTS = "disconnects"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_UART_TCP_ID): cv.use_id(UartTcp),
    cv.Optional(CONF_DISCONNECTS): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_UART_TCP_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_DISCONNECTS, hub.set_disconnects_sensor)
