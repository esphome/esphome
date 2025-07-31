from esphome import automation
import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_OPTION,
    CONF_LAMBDA,
    CONF_OPTIMISTIC,
    CONF_OPTIONS,
    CONF_RESTORE_VALUE,
    CONF_SET_ACTION,
)

from .. import template_ns

TemplateSelect = template_ns.class_(
    "TemplateSelect", select.Select, cg.PollingComponent
)


def validate(config):
    if CONF_LAMBDA in config:
        if config[CONF_OPTIMISTIC]:
            raise cv.Invalid("optimistic cannot be used with lambda")
        if CONF_INITIAL_OPTION in config:
            raise cv.Invalid("initial_value cannot be used with lambda")
        if CONF_RESTORE_VALUE in config:
            raise cv.Invalid("restore_value cannot be used with lambda")
    elif CONF_INITIAL_OPTION in config:
        if config[CONF_INITIAL_OPTION] not in config[CONF_OPTIONS]:
            raise cv.Invalid(
                f"initial_option '{config[CONF_INITIAL_OPTION]}' is not a valid option [{', '.join(config[CONF_OPTIONS])}]"
            )
    else:
        config[CONF_INITIAL_OPTION] = config[CONF_OPTIONS][0]

    if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
        raise cv.Invalid(
            "Either optimistic mode must be enabled, or set_action must be set, to handle the option being set."
        )
    return config


CONFIG_SCHEMA = cv.All(
    select.select_schema(TemplateSelect)
    .extend(
        {
            cv.Required(CONF_OPTIONS): cv.All(
                cv.ensure_list(cv.string_strict), cv.Length(min=1)
            ),
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
            cv.Optional(CONF_SET_ACTION): automation.validate_automation(single=True),
            cv.Optional(CONF_INITIAL_OPTION): cv.string_strict,
            cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s")),
    validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await select.register_select(var, config, options=config[CONF_OPTIONS])

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_template(template_))

    else:
        cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))
        cg.add(var.set_initial_option(config[CONF_INITIAL_OPTION]))

        if CONF_RESTORE_VALUE in config:
            cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [(cg.std_string, "x")], config[CONF_SET_ACTION]
        )
