from collections.abc import Callable

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
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType, SafeExpType

CODEOWNERS = ["@rfdarter", "@jesserockz"]

IS_PLATFORM_COMPONENT = True

datetime_ns = cg.esphome_ns.namespace("datetime")
DateTimeBase = datetime_ns.class_("DateTimeBase", cg.EntityBase)
DateEntity = datetime_ns.class_("DateEntity", DateTimeBase)
TimeEntity = datetime_ns.class_("TimeEntity", DateTimeBase)
DateTimeEntity = datetime_ns.class_("DateTimeEntity", DateTimeBase)

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


def _validate_time_present(config: ConfigType) -> ConfigType:
    config = config.copy()
    if CONF_ON_TIME in config and CONF_TIME_ID not in config:
        time_id = cv.use_id(time.RealTimeClock)(None)
        config[CONF_TIME_ID] = time_id
    return config


_DATETIME_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DateTimeStateTrigger),
                }
            ),
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        }
    )
    .extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
).add_extra(_validate_time_present)

_DATETIME_SCHEMA.add_extra(entity_duplicate_validator("datetime"))


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


@setup_entity("datetime")
async def setup_datetime_core_(var: MockObj, config: ConfigType) -> None:
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


async def register_datetime(var: MockObj, config: ConfigType) -> None:
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    entity_type = config[CONF_TYPE].lower()
    queue_entity_register(entity_type, config)
    CORE.register_platform_component(entity_type, var)
    await setup_datetime_core_(var, config)


async def new_datetime(config: ConfigType, *args: SafeExpType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_datetime(var, config)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(datetime_ns.using)


def _esptime_initializer(
    members: tuple[tuple[str, str], ...],
) -> Callable[[ConfigType, ConfigType], str]:
    def const_fn(config: ConfigType, value: ConfigType) -> str:
        return str(
            cg.StructInitializer(
                cg.ESPTime, *((member, value[key]) for member, key in members)
            )
        )

    return const_fn


# ESPTime member order, so the designated initializer compiles.
_TIME_MEMBERS = (("second", CONF_SECOND), ("minute", CONF_MINUTE), ("hour", CONF_HOUR))
_DATE_MEMBERS = (("day_of_month", CONF_DAY), ("month", CONF_MONTH), ("year", CONF_YEAR))

for _name, _entity, _key, _target, _date, _time, _members in (
    (
        "datetime.date.set",
        DateEntity,
        CONF_DATE,
        "set_date",
        True,
        False,
        _DATE_MEMBERS,
    ),
    (
        "datetime.time.set",
        TimeEntity,
        CONF_TIME,
        "set_time",
        False,
        True,
        _TIME_MEMBERS,
    ),
    (
        "datetime.datetime.set",
        DateTimeEntity,
        CONF_DATETIME,
        "set_datetime",
        True,
        True,
        _TIME_MEMBERS + _DATE_MEMBERS,
    ),
):
    automation.register_apply_action(
        _name,
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(_entity),
                cv.Required(_key): cv.Any(
                    cv.returning_lambda, cv.date_time(date=_date, time=_time)
                ),
            }
        ),
        automation.ApplyField(
            _key, _target, cg.ESPTime, const_fn=_esptime_initializer(_members)
        ),
        call="make_call",
    )
