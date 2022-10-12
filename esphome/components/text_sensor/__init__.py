import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_FILTERS,
    CONF_ICON,
    CONF_ID,
    CONF_ON_VALUE,
    CONF_ON_RAW_VALUE,
    CONF_TRIGGER_ID,
    CONF_MQTT_ID,
    CONF_STATE,
    CONF_FROM,
    CONF_TO,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.cpp_helpers import setup_entity
from esphome.util import Registry


IS_PLATFORM_COMPONENT = True

# pylint: disable=invalid-name
text_sensor_ns = cg.esphome_ns.namespace("text_sensor")
TextSensor = text_sensor_ns.class_("TextSensor", cg.EntityBase)
TextSensorPtr = TextSensor.operator("ptr")

TextSensorStateTrigger = text_sensor_ns.class_(
    "TextSensorStateTrigger", automation.Trigger.template(cg.std_string)
)
TextSensorStateRawTrigger = text_sensor_ns.class_(
    "TextSensorStateRawTrigger", automation.Trigger.template(cg.std_string)
)
TextSensorPublishAction = text_sensor_ns.class_(
    "TextSensorPublishAction", automation.Action
)
TextSensorStateCondition = text_sensor_ns.class_(
    "TextSensorStateCondition", automation.Condition
)

FILTER_REGISTRY = Registry()
validate_filters = cv.validate_registry("filter", FILTER_REGISTRY)

# Filters
Filter = text_sensor_ns.class_("Filter")
LambdaFilter = text_sensor_ns.class_("LambdaFilter", Filter)
ToUpperFilter = text_sensor_ns.class_("ToUpperFilter", Filter)
ToLowerFilter = text_sensor_ns.class_("ToLowerFilter", Filter)
AppendFilter = text_sensor_ns.class_("AppendFilter", Filter)
PrependFilter = text_sensor_ns.class_("PrependFilter", Filter)
SubstituteFilter = text_sensor_ns.class_("SubstituteFilter", Filter)
MapFilter = text_sensor_ns.class_("MapFilter", Filter)


@FILTER_REGISTRY.register("lambda", LambdaFilter, cv.returning_lambda)
async def lambda_filter_to_code(config, filter_id):
    lambda_ = await cg.process_lambda(
        config, [(cg.std_string, "x")], return_type=cg.optional.template(cg.std_string)
    )
    return cg.new_Pvariable(filter_id, lambda_)


@FILTER_REGISTRY.register("to_upper", ToUpperFilter, {})
async def to_upper_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id)


@FILTER_REGISTRY.register("to_lower", ToLowerFilter, {})
async def to_lower_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id)


@FILTER_REGISTRY.register("append", AppendFilter, cv.string)
async def append_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


@FILTER_REGISTRY.register("prepend", PrependFilter, cv.string)
async def prepend_filter_to_code(config, filter_id):
    return cg.new_Pvariable(filter_id, config)


def validate_mapping(value):
    if not isinstance(value, dict):
        value = cv.string(value)
        if "->" not in value:
            raise cv.Invalid("Mapping must contain '->'")
        a, b = value.split("->", 1)
        value = {CONF_FROM: a.strip(), CONF_TO: b.strip()}

    return cv.Schema(
        {cv.Required(CONF_FROM): cv.string, cv.Required(CONF_TO): cv.string}
    )(value)


@FILTER_REGISTRY.register(
    "substitute", SubstituteFilter, cv.ensure_list(validate_mapping)
)
async def substitute_filter_to_code(config, filter_id):
    from_strings = [conf[CONF_FROM] for conf in config]
    to_strings = [conf[CONF_TO] for conf in config]
    return cg.new_Pvariable(filter_id, from_strings, to_strings)


@FILTER_REGISTRY.register("map", MapFilter, cv.ensure_list(validate_mapping))
async def map_filter_to_code(config, filter_id):
    map_ = cg.std_ns.class_("map").template(cg.std_string, cg.std_string)
    return cg.new_Pvariable(
        filter_id, map_([(item[CONF_FROM], item[CONF_TO]) for item in config])
    )


TEXT_SENSOR_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMPONENT_SCHEMA).extend(
    {
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTTextSensor),
        cv.GenerateID(): cv.declare_id(TextSensor),
        cv.Optional(CONF_FILTERS): validate_filters,
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TextSensorStateTrigger),
            }
        ),
        cv.Optional(CONF_ON_RAW_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    TextSensorStateRawTrigger
                ),
            }
        ),
    }
)

_UNDEF = object()


def text_sensor_schema(
    class_: MockObjClass = _UNDEF,
    *,
    icon: str = _UNDEF,
    entity_category: str = _UNDEF,
) -> cv.Schema:
    schema = TEXT_SENSOR_SCHEMA
    if class_ is not _UNDEF:
        schema = schema.extend({cv.GenerateID(): cv.declare_id(class_)})
    if icon is not _UNDEF:
        schema = schema.extend({cv.Optional(CONF_ICON, default=icon): cv.icon})
    if entity_category is not _UNDEF:
        schema = schema.extend(
            {
                cv.Optional(
                    CONF_ENTITY_CATEGORY, default=entity_category
                ): cv.entity_category
            }
        )
    return schema


async def build_filters(config):
    return await cg.build_registry_list(FILTER_REGISTRY, config)


async def setup_text_sensor_core_(var, config):
    await setup_entity(var, config)

    if config.get(CONF_FILTERS):  # must exist and not be empty
        filters = await build_filters(config[CONF_FILTERS])
        cg.add(var.set_filters(filters))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    for conf in config.get(CONF_ON_RAW_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_text_sensor(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_text_sensor(var))
    await setup_text_sensor_core_(var, config)


async def new_text_sensor(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_text_sensor(var, config)
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_TEXT_SENSOR")
    cg.add_global(text_sensor_ns.using)


@automation.register_condition(
    "text_sensor.state",
    TextSensorStateCondition,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(TextSensor),
            cv.Required(CONF_STATE): cv.templatable(cv.string_strict),
        }
    ),
)
async def text_sensor_state_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)
    templ = await cg.templatable(config[CONF_STATE], args, cg.std_string)
    cg.add(var.set_state(templ))
    return var
