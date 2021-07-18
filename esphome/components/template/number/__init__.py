from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_OPTIMISTIC,
    CONF_STEP,
)
from .. import template_ns

TemplateNumber = template_ns.class_(
    "TemplateNumber", number.Number, cg.PollingComponent
)

CONF_SET_ACTION = "set_action"


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.NUMBER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateNumber),
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_STEP): cv.positive_float,
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
        }
    ).extend(cv.polling_component_schema("60s")),
    validate_min_max,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(float)
        )
        cg.add(var.set_template(template_))
    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [(float, "x")], config[CONF_SET_ACTION]
        )

    cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))

    cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    cg.add(var.set_max_value(config[CONF_MAX_VALUE]))
    cg.add(var.set_step(config[CONF_STEP]))
