from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_LAMBDA,
    CONF_OPTIMISTIC,
    CONF_RESTORE_VALUE,
    CONF_MAX_LENGTH,
    CONF_MIN_LENGTH,
    CONF_PATTERN,
)
from .. import template_ns

TemplateText = template_ns.class_("TemplateText", text.Text, cg.PollingComponent)

TextSaverBase = template_ns.class_("TemplateTextSaverBase")
TextSaverTemplate = template_ns.class_("TextSaver", TextSaverBase)

CONF_SET_ACTION = "set_action"
CONF_MAX_RESTORE_DATA_LENGTH = "max_restore_data_length"


def validate(config):
    if CONF_LAMBDA in config:
        if config[CONF_OPTIMISTIC]:
            raise cv.Invalid("optimistic cannot be used with lambda")
        if CONF_INITIAL_VALUE in config:
            raise cv.Invalid("initial_value cannot be used with lambda")
        if CONF_RESTORE_VALUE in config:
            raise cv.Invalid("restore_value cannot be used with lambda")
    elif CONF_INITIAL_VALUE not in config:
        config[CONF_INITIAL_VALUE] = ""

    if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
        raise cv.Invalid(
            "Either optimistic mode must be enabled, or set_action must be set, to handle the text input being set."
        )
    return config


CONFIG_SCHEMA = cv.All(
    text.TEXT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(TemplateText),
            cv.Optional(CONF_MAX_LENGTH): cv.int_,
            cv.Optional(CONF_MIN_LENGTH): cv.int_,
            cv.Optional(CONF_PATTERN): cv.string,
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_INITIAL_VALUE, default=""): cv.string_strict,
            cv.Optional(CONF_RESTORE_VALUE, default=False): cv.boolean,
            cv.Optional(CONF_MAX_RESTORE_DATA_LENGTH, default=64): cv.int_range(0, 255),
        }
    ).extend(cv.polling_component_schema("60s")),
    validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text.register_text(
        var,
        config,
    )

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_template(template_))

    else:
        cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
        cg.add(var.set_initial_value(config[CONF_INITIAL_VALUE]))
        if config[CONF_RESTORE_VALUE]:
            args = cg.TemplateArguments(config[CONF_MAX_RESTORE_DATA_LENGTH])
            saver = TextSaverTemplate.template(args).new()
            cg.add(var.set_value_saver(saver))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [(cg.std_string, "x")], config[CONF_SET_ACTION]
        )
