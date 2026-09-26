import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.types import ConfigType

from .. import template_ns

TemplateRealTimeClock = template_ns.class_("TemplateRealTimeClock", time_.RealTimeClock)


CONFIG_SCHEMA = time_.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TemplateRealTimeClock),
        cv.Required(CONF_LAMBDA): cv.returning_lambda,
    }
).extend(cv.polling_component_schema("never"))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await time_.register_time(var, config)

    template_ = await cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.int64)
    )
    cg.add(var.set_template(template_))
