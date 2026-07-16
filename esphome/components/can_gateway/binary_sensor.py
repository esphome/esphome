"""Port bus-off state binary sensor for can_gateway."""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_CAN_GATEWAY_ID, CONF_PORT_ID, CanGateway, GatewayPort

CONF_BUS_OFF = "bus_off"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CAN_GATEWAY_ID): cv.use_id(CanGateway),
        cv.Required(CONF_PORT_ID): cv.use_id(GatewayPort),
        cv.Required(CONF_BUS_OFF): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    gateway = await cg.get_variable(config[CONF_CAN_GATEWAY_ID])
    port = await cg.get_variable(config[CONF_PORT_ID])
    sens = await binary_sensor.new_binary_sensor(config[CONF_BUS_OFF])
    cg.add(gateway.set_bus_off_binary_sensor(port, sens))
