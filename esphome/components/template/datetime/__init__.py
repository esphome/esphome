from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import datetime
from esphome.const import (
    CONF_INITIAL_VALUE,
    CONF_LAMBDA,
    CONF_OPTIMISTIC,
    CONF_RESTORE_VALUE,
    CONF_SET_ACTION,
)

from esphome.core import coroutine_with_priority
from .. import template_ns

CODEOWNERS = ["@rfdarter"]


TemplateDate = template_ns.class_(
    "TemplateDate", datetime.DateEntity, cg.PollingComponent
)


def validate(config):
    config = config.copy()
    if CONF_LAMBDA in config:
        if config[CONF_OPTIMISTIC]:
            raise cv.Invalid("optimistic cannot be used with lambda")
        if CONF_INITIAL_VALUE in config:
            raise cv.Invalid("initial_value cannot be used with lambda")
        if CONF_RESTORE_VALUE in config:
            raise cv.Invalid("restore_value cannot be used with lambda")
    else:
        if CONF_RESTORE_VALUE not in config:
            config[CONF_RESTORE_VALUE] = False

    if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
        raise cv.Invalid(
            "Either optimistic mode must be enabled, or set_action must be set, to handle the date and time being set."
        )
    return config


_BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
        cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
        cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
    }
).extend(cv.polling_component_schema("60s"))

CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "DATE": datetime.date_schema(TemplateDate)
            .extend(_BASE_SCHEMA)
            .extend(
                {
                    cv.Optional(CONF_INITIAL_VALUE): cv.date_time(allowed_time=False),
                }
            ),
        },
        upper=True,
    ),
    validate,
)


@coroutine_with_priority(-100.0)
async def to_code(config):
    var = await datetime.new_datetime(config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.ESPTime)
        )
        cg.add(var.set_template(template_))

    else:
        cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
        cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))

        if initial_value := config.get(CONF_INITIAL_VALUE):
            cg.add(var.set_initial_value(initial_value))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(),
            [(cg.ESPTime, "x")],
            config[CONF_SET_ACTION],
        )

    await cg.register_component(var, config)
