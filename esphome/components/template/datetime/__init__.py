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
    CONF_DAY,
    CONF_HOUR,
    CONF_MINUTE,
    CONF_MONTH,
    CONF_SECOND,
    CONF_TYPE,
    CONF_YEAR,
)

from .. import template_ns

CODEOWNERS = ["@rfdarter"]


TemplateDate = template_ns.class_(
    "TemplateDate", datetime.DateEntity, cg.PollingComponent
)

TemplateTime = template_ns.class_(
    "TemplateTime", datetime.TimeEntity, cg.PollingComponent
)

TemplateDateTime = template_ns.class_(
    "TemplateDateTime", datetime.DateTimeEntity, cg.PollingComponent
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
                    cv.Optional(CONF_INITIAL_VALUE): cv.date_time(
                        date=True, time=False
                    ),
                }
            ),
            "TIME": datetime.time_schema(TemplateTime)
            .extend(_BASE_SCHEMA)
            .extend(
                {
                    cv.Optional(CONF_INITIAL_VALUE): cv.date_time(
                        date=False, time=True
                    ),
                }
            ),
            "DATETIME": datetime.datetime_schema(TemplateDateTime)
            .extend(_BASE_SCHEMA)
            .extend(
                {
                    cv.Optional(CONF_INITIAL_VALUE): cv.date_time(date=True, time=True),
                }
            ),
        },
        upper=True,
    ),
    validate,
)


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
            if config[CONF_TYPE] == "DATE":
                date_struct = cg.StructInitializer(
                    cg.ESPTime,
                    ("day_of_month", initial_value[CONF_DAY]),
                    ("month", initial_value[CONF_MONTH]),
                    ("year", initial_value[CONF_YEAR]),
                )
                cg.add(var.set_initial_value(date_struct))
            elif config[CONF_TYPE] == "TIME":
                time_struct = cg.StructInitializer(
                    cg.ESPTime,
                    ("second", initial_value[CONF_SECOND]),
                    ("minute", initial_value[CONF_MINUTE]),
                    ("hour", initial_value[CONF_HOUR]),
                )
                cg.add(var.set_initial_value(time_struct))
            elif config[CONF_TYPE] == "DATETIME":
                datetime_struct = cg.StructInitializer(
                    cg.ESPTime,
                    ("second", initial_value[CONF_SECOND]),
                    ("minute", initial_value[CONF_MINUTE]),
                    ("hour", initial_value[CONF_HOUR]),
                    ("day_of_month", initial_value[CONF_DAY]),
                    ("month", initial_value[CONF_MONTH]),
                    ("year", initial_value[CONF_YEAR]),
                )
                cg.add(var.set_initial_value(datetime_struct))

    if CONF_SET_ACTION in config:
        await automation.build_automation(
            var.get_set_trigger(),
            [(cg.ESPTime, "x")],
            config[CONF_SET_ACTION],
        )

    await cg.register_component(var, config)
