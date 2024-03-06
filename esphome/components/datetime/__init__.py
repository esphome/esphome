import esphome.codegen as cg

# import cpp_generator as cpp
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_ON_TIME,
    CONF_TIME_ID,
    CONF_VALUE,
    CONF_TYPE,
    CONF_MQTT_ID,
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

DATETIME_MODES = [
    "DATE",
    "TIME",
    "DATETIME",
]


def validate_datetime(config):
    if CONF_ON_TIME in config and CONF_TIME_ID not in config:
        with cv.prepend_path(CONF_ON_TIME):
            raise cv.Invalid(
                f"When using '{CONF_ON_TIME}' you need to provide '{CONF_TIME_ID}'."
            )

    return config


_DATETIME_SCHEMA = (
    cv.Schema(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTDatetimeComponent
            ),
        }
    )
    .extend(cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA))
    .add_extra(validate_datetime)
)


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


async def setup_datetime_core_(datetime_var, config):
    await setup_entity(datetime_var, config)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], datetime_var)
        await mqtt.register_mqtt_component(mqtt_, config)


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
            cv.Required(CONF_VALUE): cv.templatable(cv.date_time(allowed_time=False)),
        }
    ),
)
async def datetime_date_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    action_var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_VALUE], [], cg.ESPTime)
    cg.add(action_var.set_date(template_))
    return action_var
