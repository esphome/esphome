"""Last-RX-frame snapshot text sensor for can_gateway."""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_THROTTLE, ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_CAN_GATEWAY_ID, CONF_PORT_ID, CanGateway, GatewayPort

CONF_LAST_FRAME = "last_frame"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CAN_GATEWAY_ID): cv.use_id(CanGateway),
        cv.Required(CONF_PORT_ID): cv.use_id(GatewayPort),
        cv.Optional(CONF_THROTTLE, default="1s"): cv.positive_time_period_milliseconds,
        cv.Required(CONF_LAST_FRAME): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    # Compiles the ISR-side snapshot path in: without any
    # last_frame sensor the fast path carries zero snapshot cost.
    cg.add_define("USE_CAN_GATEWAY_SNAPSHOT")
    gateway = await cg.get_variable(config[CONF_CAN_GATEWAY_ID])
    port = await cg.get_variable(config[CONF_PORT_ID])
    sens = await text_sensor.new_text_sensor(config[CONF_LAST_FRAME])
    cg.add(
        gateway.set_last_frame_text_sensor(
            port, sens, config[CONF_THROTTLE].total_milliseconds
        )
    )
