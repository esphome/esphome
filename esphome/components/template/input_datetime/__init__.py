from esphome import automation
from datetime import datetime
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import input_datetime
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_LAMBDA,
    CONF_OPTIMISTIC,
    CONF_RESTORE_VALUE,
    CONF_SET_ACTION,
)

from .. import template_ns
from esphome.core import CORE, coroutine_with_priority


TemplateInputDatetime = template_ns.class_(
    "TemplateInputDatetime", input_datetime.InputDatetime, cg.PollingComponent
)


def validate(config):
    if CONF_LAMBDA in config:
        if config[CONF_OPTIMISTIC]:
            raise cv.Invalid("optimistic cannot be used with lambda")
        if CONF_INITIAL_VALUE in config:
            raise cv.Invalid("initial_value cannot be used with lambda")
        if CONF_RESTORE_VALUE in config:
            raise cv.Invalid("restore_value cannot be used with lambda")

    if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
        raise cv.Invalid(
            "Either optimistic mode must be enabled, or set_action must be set, to handle the date and time being set."
        )
    return config


CONFIG_SCHEMA = cv.All(
    input_datetime.input_datetime_schema(TemplateInputDatetime)
    .extend(
        {
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
            cv.Optional(CONF_INITIAL_VALUE): input_datetime.validate_timedate_value,
        }
    )
    .extend(cv.polling_component_schema("60s")),
    validate,
)


@coroutine_with_priority(-100.0)
async def to_code(config):
    input_datetime_var = cg.new_Pvariable(config[CONF_ID])

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.const_char_ptr)
        )
        cg.add(input_datetime_var.set_template(template_))

    else:
        cg.add(input_datetime_var.set_optimistic(config[CONF_OPTIMISTIC]))

        if CONF_INITIAL_VALUE in config:
            cg.add(input_datetime_var.set_initial_value(config[CONF_INITIAL_VALUE]))
            # await automation.build_automation(trigger, [], conf)
            if CONF_RESTORE_VALUE in config:
                cg.add(input_datetime_var.set_restore_value(config[CONF_RESTORE_VALUE]))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            input_datetime_var.get_set_trigger(),
            [(input_datetime.ESPTime, "x")],
            config[CONF_SET_ACTION],
        )

    await cg.register_component(input_datetime_var, config)
    await input_datetime.register_input_datetime(
        input_datetime_var,
        config,
    )
