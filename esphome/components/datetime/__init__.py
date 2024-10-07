from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt, time, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATE,
    CONF_DATETIME,
    CONF_DAY,
    CONF_HOUR,
    CONF_ID,
    CONF_MINUTE,
    CONF_MONTH,
    CONF_MQTT_ID,
    CONF_ON_TIME,
    CONF_ON_VALUE,
    CONF_SECOND,
    CONF_TIME,
    CONF_TIME_ID,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_WEB_SERVER,
    CONF_YEAR,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@rfdarter", "@jesserockz"]

IS_PLATFORM_COMPONENT = True

datetime_ns = cg.esphome_ns.namespace("datetime")
DateTimeBase = datetime_ns.class_("DateTimeBase", cg.EntityBase)
DateEntity = datetime_ns.class_("DateEntity", DateTimeBase)
TimeEntity = datetime_ns.class_("TimeEntity", DateTimeBase)
DateTimeEntity = datetime_ns.class_("DateTimeEntity", DateTimeBase)

# Actions
DateSetAction = datetime_ns.class_("DateSetAction", automation.Action)
TimeSetAction = datetime_ns.class_("TimeSetAction", automation.Action)
DateTimeSetAction = datetime_ns.class_("DateTimeSetAction", automation.Action)

DateTimeStateTrigger = datetime_ns.class_(
    "DateTimeStateTrigger", automation.Trigger.template(cg.ESPTime)
)

OnTimeTrigger = datetime_ns.class_(
    "OnTimeTrigger", automation.Trigger, cg.Component, cg.Parented.template(TimeEntity)
)
OnDateTimeTrigger = datetime_ns.class_(
    "OnDateTimeTrigger",
    automation.Trigger,
    cg.Component,
    cg.Parented.template(DateTimeEntity),
)

DATETIME_MODES = [
    "DATE",
    "TIME",
    "DATETIME",
]


def _validate_time_present(config):
    config = config.copy()
    if CONF_ON_TIME in config and CONF_TIME_ID not in config:
        time_id = cv.use_id(time.RealTimeClock)(None)
        config[CONF_TIME_ID] = time_id
    return config


_DATETIME_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    web_server.WEBSERVER_SORTING_SCHEMA,
    cv.MQTT_COMMAND_COMPONENT_SCHEMA,
    cv.Schema(
        {
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DateTimeStateTrigger),
                }
            ),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        }
    ),
).add_extra(_validate_time_present)


def date_schema(class_: MockObjClass) -> cv.Schema:
    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTDateComponent),
            cv.Optional(CONF_TYPE, default="DATE"): cv.one_of("DATE", upper=True),
        }
    )
    return _DATETIME_SCHEMA.extend(schema)


def time_schema(class_: MockObjClass) -> cv.Schema:
    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTTimeComponent),
            cv.Optional(CONF_TYPE, default="TIME"): cv.one_of("TIME", upper=True),
            cv.Optional(CONF_ON_TIME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnTimeTrigger),
                }
            ),
        }
    )
    return _DATETIME_SCHEMA.extend(schema)


def datetime_schema(class_: MockObjClass) -> cv.Schema:
    schema = cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTDateTimeComponent
            ),
            cv.Optional(CONF_TYPE, default="DATETIME"): cv.one_of(
                "DATETIME", upper=True
            ),
            cv.Optional(CONF_ON_TIME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnDateTimeTrigger),
                }
            ),
        }
    )
    return _DATETIME_SCHEMA.extend(schema)


async def setup_datetime_core_(var, config):
    await setup_entity(var, config)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)
    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)
    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.ESPTime, "x")], conf)

    if CONF_TIME_ID in config:
        rtc = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_rtc(rtc))

    for conf in config.get(CONF_ON_TIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [], conf)
        await cg.register_component(trigger, conf)
        await cg.register_parented(trigger, var)


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


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_DATETIME")
    cg.add_global(datetime_ns.using)


@automation.register_action(
    "datetime.date.set",
    DateSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(DateEntity),
            cv.Required(CONF_DATE): cv.Any(
                cv.returning_lambda, cv.date_time(date=True, time=False)
            ),
        }
    ),
)
async def datetime_date_set_to_code(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    date_config = config[CONF_DATE]
    if cg.is_template(date_config):
        template_ = await cg.templatable(date_config, args, cg.ESPTime)
        cg.add(action_var.set_date(template_))
    else:
        date_struct = cg.StructInitializer(
            cg.ESPTime,
            ("day_of_month", date_config[CONF_DAY]),
            ("month", date_config[CONF_MONTH]),
            ("year", date_config[CONF_YEAR]),
        )
        cg.add(action_var.set_date(date_struct))
    return action_var


@automation.register_action(
    "datetime.time.set",
    TimeSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(TimeEntity),
            cv.Required(CONF_TIME): cv.Any(
                cv.returning_lambda, cv.date_time(date=False, time=True)
            ),
        }
    ),
)
async def datetime_time_set_to_code(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    time_config = config[CONF_TIME]
    if cg.is_template(time_config):
        template_ = await cg.templatable(time_config, args, cg.ESPTime)
        cg.add(action_var.set_time(template_))
    else:
        time_struct = cg.StructInitializer(
            cg.ESPTime,
            ("second", time_config[CONF_SECOND]),
            ("minute", time_config[CONF_MINUTE]),
            ("hour", time_config[CONF_HOUR]),
        )
        cg.add(action_var.set_time(time_struct))
    return action_var


@automation.register_action(
    "datetime.datetime.set",
    DateTimeSetAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(DateTimeEntity),
            cv.Required(CONF_DATETIME): cv.Any(
                cv.returning_lambda, cv.date_time(date=True, time=True)
            ),
        },
    ),
)
async def datetime_datetime_set_to_code(config, action_id, template_arg, args):
    action_var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(action_var, config[CONF_ID])

    datetime_config = config[CONF_DATETIME]
    if cg.is_template(datetime_config):
        template_ = await cg.templatable(datetime_config, args, cg.ESPTime)
        cg.add(action_var.set_datetime(template_))
    else:
        datetime_struct = cg.StructInitializer(
            cg.ESPTime,
            ("second", datetime_config[CONF_SECOND]),
            ("minute", datetime_config[CONF_MINUTE]),
            ("hour", datetime_config[CONF_HOUR]),
            ("day_of_month", datetime_config[CONF_DAY]),
            ("month", datetime_config[CONF_MONTH]),
            ("year", datetime_config[CONF_YEAR]),
        )
        cg.add(action_var.set_datetime(datetime_struct))
    return action_var
