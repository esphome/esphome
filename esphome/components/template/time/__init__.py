import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.components import time
from esphome.const import CONF_ID
from .. import template_ns

CODEOWNERS = ["@RFDarter"]

TemplateRealTimeClock = template_ns.class_("TemplateRealTimeClock", time.RealTimeClock)
WriteAction = template_ns.class_("SystemTimeSetAction", automation.Action)


CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TemplateRealTimeClock),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await time.register_time(var, config)
