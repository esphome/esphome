from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_input
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_LAMBDA,
    CONF_OPTIMISTIC,
    CONF_RESTORE_VALUE,
)
from .. import template_ns

TemplateTextInput = template_ns.class_(
    "TemplateTextInput", text_input.TextInput, cg.PollingComponent
)

CONF_SET_ACTION = "set_action"


#def validate_min_max(config):
#    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
#        raise cv.Invalid("max_value must be greater than min_value")
#    return config


def validate(config):
    if CONF_LAMBDA in config:
        if config[CONF_OPTIMISTIC]:
            raise cv.Invalid("optimistic cannot be used with lambda")
        if CONF_INITIAL_VALUE in config:
            raise cv.Invalid("initial_value cannot be used with lambda")
        if CONF_RESTORE_VALUE in config:
            raise cv.Invalid("restore_value cannot be used with lambda")
    elif CONF_INITIAL_VALUE not in config:
        config[CONF_INITIAL_VALUE] = config[CONF_MIN_VALUE]

    if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
        raise cv.Invalid(
            "Either optimistic mode must be enabled, or set_action must be set, to handle the text input being set."
        )
    return config


CONFIG_SCHEMA = cv.All(
    text_input.TEXT_INPUT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateTextInput),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_INITIAL_VALUE): cv.float_,
            cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
        }
    ).extend(cv.polling_component_schema("60s")),
    validate_min_max,
    validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_input.register_text_input(
        var,
        config,
#        min_value=config[CONF_MIN_VALUE],
#        max_value=config[CONF_MAX_VALUE],
#        step=config[CONF_STEP],
    )

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_template(template_))

    else:
        cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
        cg.add(var.set_initial_value(config[CONF_INITIAL_VALUE]))
        if CONF_RESTORE_VALUE in config:
            cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [(cg.std_string, "x")], config[CONF_SET_ACTION]
        )
