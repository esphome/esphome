import logging
import re
from importlib import resources
from typing import Optional

import esphome.codegen as cg

# import cpp_generator as cpp
import esphome.config_validation as cv
from esphome import automation
from esphome.components import time
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_ON_TIME,
    CONF_TRIGGER_ID,
    CONF_INITIAL_VALUE,
    CONF_TIME,
    CONF_TIME_ID,
    CONF_ON_VALUE,
    CONF_VALUE,
    CONF_MODE,
    CONF_MQTT_ID,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.automation import Condition
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity


ESP_TIME_VAR = "esptime_var"

_LOGGER = logging.getLogger(__name__)

# bases of time from OttoWinter

CODEOWNERS = ["@rfdarter"]
IS_PLATFORM_COMPONENT = True

datetime_ns = cg.esphome_ns.namespace("datetime")
Datetime = datetime_ns.class_("Datetime", cg.EntityBase)
DatetimeOnTimeTrigger = datetime_ns.class_(
    "DatetimeOnTimeTrigger", automation.Trigger.template(), cg.Component
)
ESPTime = cg.esphome_ns.struct("ESPTime")

# Triggers
DatetimeStateTrigger = datetime_ns.class_(
    "DatetimeStateTrigger",
    automation.Trigger.template(ESPTime, cg.bool_, cg.bool_),
)

# Actions
DatetimeSetAction = datetime_ns.class_("DatetimeSetAction", automation.Action)

# Conditions
DatetimeHasTimeCondition = datetime_ns.class_("DatetimeHasTimeCondition", Condition)
DatetimeHasDateCondition = datetime_ns.class_("DatetimeHasDateCondition", Condition)

DatetimeMode = datetime_ns.enum("DatetimeMode")

DATETIME_MODES = {
    "AUTO": DatetimeMode.DATETIME_MODE_AUTO,
}


def has_datetime_string_date_only(value):
    return bool(re.match(r"^\d{4}-\d{2}-\d{2}$", value))


def has_datetime_string_time_only(value):
    return bool(re.match(r"^\d{2}:\d{2}(:\d{2})?$", value))


def has_datetime_string_date_and_time(value):
    return bool(re.match(r"^(\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}(:\d{2})?)$", value))


def has_datetime_string_date_or_time(value):
    return bool(
        re.match(
            r"^(\d{4}-\d{2}-\d{2}([ T]\d{2}:\d{2}(:\d{2})?)?|\d{2}:\d{2}(:\d{2})?)$",
            value,
        )
    )


def validate_datetime_string(value: str):
    if not has_datetime_string_date_or_time(value):
        raise cv.Invalid(
            "Not a valid datetime: '{0}'.".format(value)
            + " valid formats are '2024-05-28 16:45:15', '2024-05-28', '16:45:15', '16:45'"
        )
    return value


def valdiate_time_string(time_string: str, has_date: bool, has_time: bool):
    validate_datetime_string(time_string)

    if not has_date and not has_time:
        raise cv.Invalid(
            "Trying to set datetime value, but neither 'has_date' nor 'has_time' was set"
        )
    elif (
        has_date
        and has_time
        and (
            has_datetime_string_date_only(time_string)
            or has_datetime_string_time_only(time_string)
        )
    ):
        raise cv.Invalid(
            "Time string was provided in the wrong format! Expected date and time. '2024-05-28 16:45:15'"
        )
    elif (
        has_date
        and not has_time
        and (
            has_datetime_string_time_only(time_string)
            or has_datetime_string_date_and_time(time_string)
        )
    ):
        raise cv.Invalid(
            "Time string was provided in the wrong format! Expected date only. '2024-05-28'"
        )
    elif (
        not has_date
        and has_time
        and (
            has_datetime_string_date_only(time_string)
            or has_datetime_string_date_and_time(time_string)
        )
    ):
        raise cv.Invalid(
            "Time string was provided in the wrong format! Expected time only. '16:45:15' or '16:45'"
        )


def validate_datetime(config):
    print(config)
    if CONF_ON_TIME in config and CONF_TIME_ID not in config:
        raise cv.Invalid(
            f"When using '{CONF_ON_TIME}' you need to provide '{CONF_TIME_ID}'."
        )

    if CONF_INITIAL_VALUE in config:
        validate_datetime_string(config[CONF_INITIAL_VALUE])

    return config


# DATETIME_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
#     {
#         cv.Optional(CONF_TIME_ID): cv.All( cv.requires_component(CONF_TIME), cv.use_id(time.RealTimeClock) ),
#         cv.Optional(CONF_HAS_DATE, False): cv.boolean,
#         cv.Optional(CONF_HAS_TIME, False): cv.boolean,
#         cv.Optional(CONF_ON_VALUE): automation.validate_automation(
#             {
#                 cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DatetimeStateTrigger),
#             }
#         ),
#         cv.Optional(CONF_ON_TIME): automation.validate_automation(
#             {
#                 cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DatetimeOnTimeTrigger),
#             }
#         ),
#     }

# ).extend(cv.polling_component_schema("15min")).add_extra(validate_datetime)

DATETIME_SCHEMA = (
    cv.Schema(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTDatetimeComponent
            ),
            cv.Optional(CONF_TIME_ID): cv.All(
                cv.requires_component(CONF_TIME), cv.use_id(time.RealTimeClock)
            ),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DatetimeStateTrigger),
                }
            ),
            cv.Optional(CONF_ON_TIME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        DatetimeOnTimeTrigger
                    ),
                }
            ),
            cv.Optional(CONF_MODE, default="AUTO"): cv.enum(DATETIME_MODES, upper=True),
        }
    )
    .add_extra(validate_datetime)
    .extend(cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA))
)


def datetime_schema(class_: MockObjClass) -> cv.Schema:
    schema = {cv.GenerateID(): cv.declare_id(class_)}
    return DATETIME_SCHEMA.extend(schema)


async def setup_datetime_core_(datetime_var, config):
    await setup_entity(datetime_var, config)

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], datetime_var)
        await automation.build_automation(trigger, [(ESPTime, "x")], conf)

    if CONF_ON_TIME in config:
        time_var = await cg.get_variable(config[CONF_TIME_ID])
        for conf in config.get(CONF_ON_TIME, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], datetime_var, time_var)
            await cg.register_component(trigger, conf)
            # await cg.register_component(trigger, conf)
            await automation.build_automation(trigger, [], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], datetime_var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_datetime(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.DatetimeMode(var))
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
        cv.Required(CONF_ID): cv.use_id(Datetime),
    }
)


@automation.register_action(
    "datetime.set",
    DatetimeSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): validate_datetime_string,
        }
    ),
)
async def datetime_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    action_var = cg.new_Pvariable(action_id, template_arg, paren)

    # await add_datetime_set_value(var, config[CONF_VALUE], config)
    cg.add(action_var.set_value(config[CONF_VALUE]))
    return action_var


@automation.register_condition(
    "datetime.has_time",
    DatetimeHasTimeCondition,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Datetime),
        }
    ),
)
async def datetime_has_time_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


@automation.register_condition(
    "datetime.has_date",
    DatetimeHasDateCondition,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(Datetime),
        }
    ),
)
async def datetime_has_date_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
