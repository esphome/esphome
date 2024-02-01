import logging
import re
from importlib import resources
from typing import Optional
from datetime import datetime as dt

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

ESPTime = cg.esphome_ns.struct("ESPTime")

# Triggers
DatetimeOnTimeTrigger = datetime_ns.class_(
    "DatetimeOnTimeTrigger", automation.Trigger.template(), cg.Component
)

DatetimeValueTrigger = datetime_ns.class_(
    "DatetimeValueTrigger",
    automation.Trigger.template(ESPTime, cg.bool_, cg.bool_),
)

# Actions
DatetimeSetAction = datetime_ns.class_("DatetimeSetAction", automation.Action)

# Conditions
DatetimeHasDateCondition = datetime_ns.class_("DatetimeHasDateCondition", Condition)
DatetimeHasTimeCondition = datetime_ns.class_("DatetimeHasTimeCondition", Condition)

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


# does not take iinto account that some months do have < 31 days, do we need to check that?
def validate_datetime_string(value: str):
    if not has_datetime_string_date_or_time(value):
        raise cv.Invalid(
            "Not a valid datetime: '{0}'.".format(value)
            + " valid formats are '2024-05-28 16:45:15', '2024-05-28', '16:45:15', '16:45'"
        )

    if len(value.split(":")) == 2:
        value += ":00"

    format = ""
    invalide_valus = []
    if has_datetime_string_date_and_time(value):
        format = ["%Y", "%m", "%d", "%H", "%M", "%S"]
    if has_datetime_string_date_only(value):
        format = ["%Y", "%m", "%d"]
    if has_datetime_string_time_only(value):
        format = ["%H", "%M", "%S"]

    splitted_value = re.split(r"[- :]", value)
    for i, val in enumerate(splitted_value):
        try:
            dt.strptime(val, format[i])
        except ValueError:
            invalide_valus.append([val, format[i]])
    if len(invalide_valus):
        format_to_string = {
            "%Y": ["Year", "0001-9999"],
            "%m": ["Month", "1-12"],
            "%d": ["Day", "1-31"],
            "%H": ["Hour", "0-23"],
            "%M": ["Minute", "0-59"],
            "%S": ["Second", "0-59"],
        }
        invalide_component = ""
        for invalide_value in invalide_valus:
            value = value.replace(invalide_value[0], f"{{{invalide_value[0]}}}")

            invalide_component += f"\n{invalide_value[0]} is not valid for component '{format_to_string[invalide_value[1]][0]}'({format_to_string[invalide_value[1]][1]})"
        raise cv.Invalid(
            "Not a valid datetime: '{0}'.".format(value) + f"{invalide_component}"
        )
    return value


def validate_datetime(config):
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
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DatetimeValueTrigger),
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
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

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
        cv.Required(CONF_ID): cv.use_id(Datetime),
    }
)


@automation.register_action(
    "datetime.set",
    DatetimeSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): cv.templatable(validate_datetime_string),
        }
    ),
)
async def datetime_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    action_var = cg.new_Pvariable(action_id, template_arg, paren)

    # await add_datetime_set_value(var, config[CONF_VALUE], config)
    template_ = await cg.templatable(config[CONF_VALUE], [], cg.std_string)
    cg.add(action_var.set_value(template_))
    return action_var


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
