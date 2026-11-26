"""Profile component for the canopen tests: one other node, through canopen.register_node()."""

import esphome.codegen as cg
from esphome.components import canopen
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["canopen"]
DOMAIN = "fake_canopen_profile"
MULTI_CONF = True

CONF_HEARTBEAT_TIMEOUT = "heartbeat_timeout"

ns = cg.esphome_ns.namespace("fake_canopen_profile")
FakeProfile = ns.class_("FakeProfile", cg.Component, canopen.CANopenNode)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FakeProfile),
            cv.Optional(CONF_HEARTBEAT_TIMEOUT): canopen.UINT16_MS,
        }
    )
    .extend(canopen.CANOPEN_NODE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)
FINAL_VALIDATE_SCHEMA = canopen.final_validate_canopen_node


async def to_code(config):
    hub = await cg.get_variable(config[canopen.CONF_CANOPEN_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub, config[canopen.CONF_NODE_ID])
    await cg.register_component(var, config)
    await canopen.register_node(
        var,
        config,
        sdo=True,
        nmt=True,
        heartbeat_timeout=config.get(CONF_HEARTBEAT_TIMEOUT),
    )
