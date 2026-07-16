"""Gateway enable switch for can_gateway."""

from __future__ import annotations

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_CAN_GATEWAY_ID, CanGateway, can_gateway_ns

CanGatewaySwitch = can_gateway_ns.class_(
    "CanGatewaySwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = switch.switch_schema(
    CanGatewaySwitch,
    default_restore_mode="RESTORE_DEFAULT_ON",
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:swap-horizontal",
).extend(
    {
        cv.GenerateID(CONF_CAN_GATEWAY_ID): cv.use_id(CanGateway),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CAN_GATEWAY_ID])
