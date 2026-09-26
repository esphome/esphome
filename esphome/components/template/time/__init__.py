import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_SYNC,
    CONF_UPDATE_INTERVAL,
    SCHEDULER_DONT_RUN,
)
from esphome.types import ConfigType

from .. import template_ns

TemplateRealTimeClock = template_ns.class_("TemplateRealTimeClock", time_.RealTimeClock)


def _validate_sync(config: ConfigType) -> ConfigType:
    if (
        config[CONF_SYNC]
        and config[CONF_UPDATE_INTERVAL].total_milliseconds == SCHEDULER_DONT_RUN
    ):
        raise cv.Invalid(f"'{CONF_SYNC}: true' requires an '{CONF_UPDATE_INTERVAL}'")
    return config


CONFIG_SCHEMA = cv.All(
    time_.TIME_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateRealTimeClock),
            cv.Required(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_SYNC, default=False): cv.boolean,
        }
    ).extend(cv.polling_component_schema("never")),
    _validate_sync,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await time_.register_time(var, config)

    template_ = await cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.int64)
    )
    cg.add(var.set_template(template_))
    cg.add(var.set_sync_system_time(config[CONF_SYNC]))
