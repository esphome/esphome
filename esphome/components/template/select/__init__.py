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
    CONF_UPDATE_INTERVAL,
    SCHEDULER_DONT_RUN,
)
from esphome.core import TimePeriodMilliseconds
from esphome.cpp_generator import TemplateArguments

from .. import template_ns

TemplateSelect = template_ns.class_(
    "TemplateSelect", select.Select, cg.PollingComponent
)
TemplateSelectWithSetAction = template_ns.class_(
    "TemplateSelectWithSetAction", TemplateSelect
)


def validate(config):
    errors = []
    if CONF_LAMBDA in config:
        if config[CONF_OPTIMISTIC]:
            errors.append(
                cv.Invalid(
                    "optimistic cannot be used with lambda", path=[CONF_OPTIMISTIC]
                )
            )
        if CONF_INITIAL_OPTION in config:
            errors.append(
                cv.Invalid(
                    "initial_value cannot be used with lambda",
                    path=[CONF_INITIAL_OPTION],
                )
            )
        if CONF_RESTORE_VALUE in config:
            errors.append(
                cv.Invalid(
                    "restore_value cannot be used with lambda",
                    path=[CONF_RESTORE_VALUE],
                )
            )
    elif CONF_INITIAL_OPTION in config:
        if config[CONF_INITIAL_OPTION] not in config[CONF_OPTIONS]:
            errors.append(
                cv.Invalid(
                    f"initial_option '{config[CONF_INITIAL_OPTION]}' is not a valid option [{', '.join(config[CONF_OPTIONS])}]",
                    path=[CONF_INITIAL_OPTION],
                )
            )
    else:
        config[CONF_INITIAL_OPTION] = config[CONF_OPTIONS][0]

    if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
        errors.append(
            cv.Invalid(
                "Either optimistic mode must be enabled, or set_action must be set, to handle the option being set."
            )
        )
    if errors:
        raise cv.MultipleInvalid(errors)

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
    var_id = config[CONF_ID]
    if CONF_SET_ACTION in config:
        var_id.type = TemplateSelectWithSetAction
    has_lambda = CONF_LAMBDA in config
    optimistic = config.get(CONF_OPTIMISTIC, False)
    restore_value = config.get(CONF_RESTORE_VALUE, False)
    options = config[CONF_OPTIONS]
    initial_option = config.get(CONF_INITIAL_OPTION, 0)
    initial_option_index = options.index(initial_option) if not has_lambda else 0

    var = cg.new_Pvariable(
        var_id,
        TemplateArguments(has_lambda, optimistic, restore_value, initial_option_index),
    )
    component_config = config.copy()
    if not has_lambda:
        # No point in polling if not using a lambda
        component_config[CONF_UPDATE_INTERVAL] = TimePeriodMilliseconds(
            milliseconds=SCHEDULER_DONT_RUN
        )
    await cg.register_component(var, component_config)
    await select.register_select(var, config, options=options)

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [], return_type=cg.optional.template(cg.std_string)
        )
        cg.add(var.set_lambda(lambda_))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(), [(cg.StringRef, "x")], config[CONF_SET_ACTION]
        )
