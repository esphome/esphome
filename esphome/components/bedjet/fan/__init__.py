import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv

from .. import BEDJET_CLIENT_SCHEMA, bedjet_ns, register_bedjet_child

CODEOWNERS = ["@jhansche"]
DEPENDENCIES = ["bedjet"]

BedJetFan = bedjet_ns.class_("BedJetFan", fan.Fan, cg.PollingComponent)

CONFIG_SCHEMA = (
    fan.fan_schema(BedJetFan)
    .extend(cv.polling_component_schema("60s"))
    .extend(BEDJET_CLIENT_SCHEMA)
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await register_bedjet_child(var, config)
