import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan
from esphome.const import (
    CONF_ID,
)
from .. import (
    BEDJET_CLIENT_SCHEMA,
    bedjet_ns,
    register_bedjet_child,
)

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["bedjet"]

BedJetFan = bedjet_ns.class_("BedJetFan", fan.Fan, cg.PollingComponent)

CONFIG_SCHEMA = (
    fan.FAN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BedJetFan),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(BEDJET_CLIENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    await register_bedjet_child(var, config)
