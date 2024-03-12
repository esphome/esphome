import esphome.codegen as cg

# import cpp_generator as cpp
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_ON_VALUE,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_MQTT_ID,
    CONF_DATE,
    CONF_YEAR,
    CONF_MONTH,
    CONF_DAY,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity


CODEOWNERS = ["@rfdarter"]

IS_PLATFORM_COMPONENT = True

datetime_ns = cg.esphome_ns.namespace("datetime")
DateTimeBase = datetime_ns.class_("DateTimeBase", cg.EntityBase)
DateEntity = datetime_ns.class_("DateEntity", DateTimeBase)

# Actions
DateSetAction = datetime_ns.class_("DateSetAction", automation.Action)

DateTimeStateTrigger = datetime_ns.class_(
    "DateTimeStateTrigger", automation.Trigger.template(cg.ESPTime)
)

DATETIME_MODES = [
    "DATE",
    "TIME",
    "DATETIME",
]


_DATETIME_SCHEMA = cv.Schema(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTDatetimeComponent),
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DateTimeStateTrigger),
            }
        ),
    }
).extend(cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA))


def date_schema(class_: MockObjClass) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
        cv.Optional(CONF_TYPE, default="DATE"): cv.one_of("DATE", upper=True),
    }
    return _DATETIME_SCHEMA.extend(schema)


def time_schema(class_: MockObjClass) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
        cv.Optional(CONF_TYPE, default="TIME"): cv.one_of("TIME", upper=True),
    }
    return _DATETIME_SCHEMA.extend(schema)


def datetime_schema(class_: MockObjClass) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
        cv.Optional(CONF_TYPE, default="DATETIME"): cv.one_of("DATETIME", upper=True),
    }
    return _DATETIME_SCHEMA.extend(schema)


async def setup_datetime_core_(var, config):
    await setup_entity(var, config)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)
    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.ESPTime, "x")], conf)


async def register_datetime(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(getattr(cg.App, f"register_{config[CONF_TYPE].lower()}")(var))
    await setup_datetime_core_(var, config)
    cg.add_define(f"USE_DATETIME_{config[CONF_TYPE]}")


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
        cv.Required(CONF_ID): cv.use_id(DateEntity),
    }
)


@automation.register_action(
    "datetime.date.set",
    DateSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_DATE): cv.Any(
                cv.returning_lambda, cv.date_time(allowed_time=False)
            ),
        }
    ),
)
async def datetime_date_set_to_code(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    date = config[CONF_DATE]
    if cg.is_template(date):
        template_ = await cg.templatable(config[CONF_DATE], [], cg.ESPTime)
        cg.add(action_var.set_date(template_))
    else:
        date_struct = cg.StructInitializer(
            cg.ESPTime,
            ("day_of_month", date[CONF_DAY]),
            ("month", date[CONF_MONTH]),
            ("year", date[CONF_YEAR]),
        )
        cg.add(action_var.set_date(date_struct))
    return action_var
