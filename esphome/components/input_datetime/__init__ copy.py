import logging
from importlib import resources
from typing import Optional

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome import automation
from esphome.components import time
from esphome.const import (
    CONF_ID,
    CONF_CRON,
    CONF_DAYS_OF_MONTH,
    CONF_DAYS_OF_WEEK,
    CONF_HOURS,
    CONF_MINUTES,
    CONF_MONTHS,
    CONF_ON_TIME,
    CONF_SECONDS,
    CONF_TRIGGER_ID,
    CONF_AT,
    CONF_SECOND,
    CONF_HOUR,
    CONF_MINUTE,
    CONF_INITIAL_VALUE,
    CONF_TIME,
    CONF_TIME_ID,
    CONF_ON_VALUE,
    CONF_VALUE,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.automation import Condition
from esphome.cpp_generator import MockObjClass

# bases of time from OttoWinter

CONF_HAS_TIME = "has_time"
CONF_HAS_DATE = "has_date"
CONF_DAY_OF_MONTH = "day_of_month"
CONF_DAY_OF_WEEK = "day_of_week"
CONF_MONTH = "month"

ESP_TIME_VAR = "esptime_var"

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@rfdarter"]
IS_PLATFORM_COMPONENT = True

datetime_ns = cg.esphome_ns.namespace("datetime")
InputDatetime = datetime_ns.class_("InputDatetime", cg.EntityBase)
InputDatetimeOnTimeTrigger = datetime_ns.class_(
    "InputDatetimeOnTimeTrigger", automation.Trigger.template(), cg.Component
)
ESPTime = cg.esphome_ns.struct("ESPTime")

# Triggers
InputDatetimeStateTrigger = datetime_ns.class_(
    "InputDatetimeStateTrigger",
    automation.Trigger.template(ESPTime, cg.bool_, cg.bool_),
)

# Actions
InputDatetimeSetAction = datetime_ns.class_(
    "InputDatetimeSetAction", automation.Action
)

# Conditions
InputDatetimeHasTimeCondition = datetime_ns.class_(
    "InputDatetimeHasTimeCondition", Condition
)
InputDatetimeHasDateCondition = datetime_ns.class_(
    "InputDatetimeHasDateCondition", Condition
)


def _parse_cron_int(value, special_mapping, message):
    special_mapping = special_mapping or {}
    if isinstance(value, str) and value in special_mapping:
        return special_mapping[value]
    try:
        return int(value)
    except ValueError:
        # pylint: disable=raise-missing-from
        raise cv.Invalid(message.format(value))


def cron_expression_validator(name, min_value, max_value, special_mapping=None):
    def validator(value):
        value = _parse_cron_int(
            value,
            special_mapping,
            "Number for time expression must be an integer, got {}",
        )
        if not isinstance(value, int):
            raise cv.Invalid(
                f"Expected integer for {value} '{name}', got {type(value)}"
            )
        if value < min_value or value > max_value:
            raise cv.Invalid(
                f"{name} {value} is out of range (min={min_value} max={max_value})."
            )
        return value

    return validator


validate_cron_second = cron_expression_validator("second", 0, 60)
validate_cron_minute = cron_expression_validator("minute", 0, 59)
validate_cron_hour = cron_expression_validator("hour", 0, 23)
validate_cron_days_of_month = cron_expression_validator("day of month", 1, 31)
validate_cron_month = cron_expression_validator(
    "month",
    1,
    12,
    {
        "JAN": 1,
        "FEB": 2,
        "MAR": 3,
        "APR": 4,
        "MAY": 5,
        "JUN": 6,
        "JUL": 7,
        "AUG": 8,
        "SEP": 9,
        "OCT": 10,
        "NOV": 11,
        "DEC": 12,
    },
)

validate_cron_day_of_week = cron_expression_validator(
    "day of week",
    1,
    7,
    {"SUN": 1, "MON": 2, "TUE": 3, "WED": 4, "THU": 5, "FRI": 6, "SAT": 7},
)

# def validate(config):
#     if CONF_LAMBDA in config:
#         if config[CONF_OPTIMISTIC]:
#             raise cv.Invalid("optimistic cannot be used with lambda")
#         if CONF_INITIAL_VALUE in config:
#             raise cv.Invalid("initial_value cannot be used with lambda")
#         if CONF_RESTORE_VALUE in config:
#             raise cv.Invalid("restore_value cannot be used with lambda")
#     elif CONF_INITIAL_VALUE not in config:
#         config[CONF_INITIAL_VALUE] = {0}

#     if not config[CONF_OPTIMISTIC] and CONF_SET_ACTION not in config:
#         raise cv.Invalid(
#             "Either optimistic mode must be enabled, or set_action must be set, to handle the number being set."
#         )
#     return config
VALID_INITAL_VALUES = [
    CONF_SECOND,
    CONF_MINUTE,
    CONF_HOUR,
    CONF_DAY_OF_MONTH,
    CONF_DAY_OF_WEEK,
    CONF_MONTH,
]


def validate_datetime(config):
    print("===================================================")
    print(config)
    print("=============================================k======")
    if CONF_INITIAL_VALUE in config:
        conf_inital_value = config.get(CONF_INITIAL_VALUE)
        if (
            CONF_DAY_OF_WEEK in conf_inital_value
            and CONF_DAY_OF_MONTH in conf_inital_value
        ):
            raise cv.Invalid(
                f"Cannot use {CONF_DAY_OF_WEEK} and {CONF_DAY_OF_MONTH} at the same time."
            )

    if CONF_ON_TIME in config and not CONF_TIME_ID in config:
        raise cv.Invalid(
            f"When using {CONF_ON_TIME} you need to provide {CONF_TIME_ID}."
        )

    return config


def validate_timedate_value(config):
    # print(config)

    if CONF_DAY_OF_WEEK in config and CONF_DAY_OF_MONTH in config:
        raise cv.Invalid(
            f"Cannot use {CONF_DAY_OF_WEEK} and {CONF_DAY_OF_MONTH} at the same time."
        )
    return cv.has_at_least_one_key(*VALID_INITAL_VALUES)(config)


def validate_has_time_date(config):
    # print("========================================================================")
    print(config)
    # print("========================================================================")
    return cv.has_at_least_one_key(CONF_HAS_TIME, CONF_HAS_DATE)(config)


# TIME_DATE_VALUE_SCHEMA = cv.Schema(
#     cv.All(
#         {
#             cv.Optional(CONF_SECOND): cv.int_range(0, 59),
#             cv.Optional(CONF_MINUTE): cv.int_range(0, 59),
#             cv.Optional(CONF_HOUR): cv.int_range(0, 23),
#             cv.Optional(CONF_DAY_OF_MONTH): cv.int_range(0, 31),
#             cv.Optional(CONF_DAY_OF_WEEK): validate_cron_day_of_week,
#             cv.Optional(CONF_MONTH): validate_cron_month,
#         },
#         validate_timedate_value )
# )

TIME_DATE_VALUE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SECOND): cv.int_range(0, 59),
        cv.Optional(CONF_MINUTE): cv.int_range(0, 59),
        cv.Optional(CONF_HOUR): cv.int_range(0, 23),
        cv.Optional(CONF_DAY_OF_MONTH): cv.int_range(0, 31),
        cv.Optional(CONF_MONTH): validate_cron_month,
    }
)

# has_at_least_one_key
INPUT_DATETIME_SCHEMA = (
    cv.Schema({})
    .extend(
        cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
        .extend(
            {
                cv.Optional(CONF_TIME_ID): cv.All(
                    cv.requires_component(CONF_TIME), cv.use_id(time.RealTimeClock)
                ),
                cv.Optional(CONF_HAS_TIME, False): cv.boolean,
                cv.Optional(CONF_HAS_DATE, False): cv.boolean,
                cv.Optional(CONF_INITIAL_VALUE): cv.All(
                    TIME_DATE_VALUE_SCHEMA, validate_timedate_value
                ),
                cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            InputDatetimeStateTrigger
                        ),
                    }
                ),
                cv.Optional(CONF_ON_TIME): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            InputDatetimeOnTimeTrigger
                        ),
                    }
                ),
            }
        )
        .extend(cv.polling_component_schema("15min"))
    )
    .add_extra(validate_datetime)
)


def datetime_schema(class_: MockObjClass) -> cv.Schema:
    schema = {cv.GenerateID(): cv.declare_id(class_)}
    return INPUT_DATETIME_SCHEMA.extend(schema)


async def setup_datetime_core_(datetime_var, config):
    time_var = await cg.get_variable(config[CONF_TIME_ID])

    # print(config)

    conf_inital_value = config.get(CONF_INITIAL_VALUE)
    print(config)
    print(conf_inital_value)
    # print(conf_inital_value)
    if conf_inital_value != {0}:
        print("true")
        second = conf_inital_value.get(CONF_SECOND)
        minute = conf_inital_value.get(CONF_MINUTE)
        hour = conf_inital_value.get(CONF_HOUR)
        days_of_month = conf_inital_value.get(
            CONF_DAYS_OF_MONTH,
        )
        month = conf_inital_value.get(
            CONF_MONTHS,
        )
        print(conf_inital_value)
        print("sec:" + str(second))
        print("min:" + str(minute))
        print("hour:" + str(hour))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], datetime_var)
        await automation.build_automation(trigger, [(ESPTime, "x")], conf)

    trigger_only_once = False
    for conf in config.get(CONF_ON_TIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], datetime_var, time_var)
        if not trigger_only_once:
            trigger_only_once = True
            await cg.register_component(trigger, conf)

        # await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)


async def register_datetime(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_datetime(var))
    await setup_datetime_core_(var, config)


async def new_datetime(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_datetime(var, config)
    return var


@coroutine_with_priority(40.0)
async def to_code(config):
    cg.add_define("USE_DATETIME")
    cg.add_global(datetime_ns.using)


OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(InputDatetime),
    }
)


async def add_datetime_set_value(action_var, config):
    if not CORE.has_id(ID(ESP_TIME_VAR)):
        esptime_var = cg.new_Pvariable(
            ID(ESP_TIME_VAR, False, ESPTime),
        )
    else:
        esptime_var = await cg.get_variable(ID(ESP_TIME_VAR))
    #    esptimeex_var = esptimeex_var.operator("ptr")
    #    cg.MockObj
    has_date = False
    has_time = False
    time_inputs = {
        "second": 0,
        "minute": 0,
        "day_of_week": 0,
        "day_of_month": 0,
        "month": 0,
    }

    if CONF_SECOND in config:
        time_inputs[CONF_SECOND] = config[CONF_SECOND]
    if CONF_MINUTE in config:
        time_inputs[CONF_MINUTE] = config[CONF_MINUTE]
    if CONF_HOUR in config:
        time_inputs[CONF_HOUR] = config[CONF_HOUR]
    if CONF_DAY_OF_WEEK in config:
        time_inputs[CONF_DAY_OF_WEEK] = config[CONF_DAY_OF_WEEK]
    if CONF_DAY_OF_MONTH in config:
        time_inputs[CONF_DAY_OF_MONTH] = config[CONF_DAY_OF_MONTH]
    if CONF_HAS_DATE in config:
        has_date = True
    if CONF_HAS_TIME in config:
        has_time = True
    for entry in time_inputs:
        cg.add(cg.RawExpression(f"{esptime_var}->{entry} = {time_inputs[entry]}"))

    # cg.add(cg.RawExpression(F"{action_var}->set_value(*{esptimeex_var})") )
    if has_date and not has_time:
        cg.add(action_var.set_date(cg.MockObj(f"*{esptime_var}", "")))
    elif has_time and not has_date:
        cg.add(action_var.set_time(cg.MockObj(f"*{esptime_var}", "")))
    elif has_date and has_time:
        cg.add(action_var.set_datetime(cg.MockObj(f"*{esptime_var}", "")))


@automation.register_action(
    "datetime.set",
    InputDatetimeSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): validate_timedate_value,
        }
    ).add_extra(validate_timedate_value),
)
async def datetime_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    await add_datetime_set_value(var, config)

    return var


@automation.register_condition(
    "datetime.has_time",
    InputDatetimeHasTimeCondition,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(InputDatetime),
        }
    ),
)
async def datetime_has_time_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


@automation.register_condition(
    "datetime.has_date",
    InputDatetimeHasDateCondition,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(InputDatetime),
        }
    ),
)
async def datetime_has_date_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
