import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import mqtt
from esphome.components import web_server
from esphome.const import (
    CONF_ABOVE,
    CONF_BELOW,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_ICON,
    CONF_MODE,
    CONF_ON_VALUE,
    CONF_ON_VALUE_RANGE,
    CONF_TRIGGER_ID,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_MQTT_ID,
    CONF_VALUE,
    CONF_OPERATION,
    CONF_CYCLE,
    CONF_WEB_SERVER_ID,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_CONDUCTIVITY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DATA_RATE,
    DEVICE_CLASS_DATA_SIZE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MONETARY,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_NITROGEN_MONOXIDE,
    DEVICE_CLASS_NITROUS_OXIDE,
    DEVICE_CLASS_OZONE,
    DEVICE_CLASS_PH,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_PRECIPITATION,
    DEVICE_CLASS_PRECIPITATION_INTENSITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_SOUND_PRESSURE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_SULPHUR_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_VOLUME,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_VOLUME_STORAGE,
    DEVICE_CLASS_WATER,
    DEVICE_CLASS_WEIGHT,
    DEVICE_CLASS_WIND_SPEED,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@esphome/core"]
DEVICE_CLASSES = [
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    DEVICE_CLASS_CONDUCTIVITY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_DATA_RATE,
    DEVICE_CLASS_DATA_SIZE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_ENERGY_STORAGE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    DEVICE_CLASS_MOISTURE,
    DEVICE_CLASS_MONETARY,
    DEVICE_CLASS_NITROGEN_DIOXIDE,
    DEVICE_CLASS_NITROGEN_MONOXIDE,
    DEVICE_CLASS_NITROUS_OXIDE,
    DEVICE_CLASS_OZONE,
    DEVICE_CLASS_PH,
    DEVICE_CLASS_PM1,
    DEVICE_CLASS_PM10,
    DEVICE_CLASS_PM25,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_PRECIPITATION,
    DEVICE_CLASS_PRECIPITATION_INTENSITY,
    DEVICE_CLASS_PRESSURE,
    DEVICE_CLASS_REACTIVE_POWER,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_SOUND_PRESSURE,
    DEVICE_CLASS_SPEED,
    DEVICE_CLASS_SULPHUR_DIOXIDE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_VOLUME,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_VOLUME_STORAGE,
    DEVICE_CLASS_WATER,
    DEVICE_CLASS_WEIGHT,
    DEVICE_CLASS_WIND_SPEED,
]
IS_PLATFORM_COMPONENT = True

number_ns = cg.esphome_ns.namespace("number")
Number = number_ns.class_("Number", cg.EntityBase)
NumberPtr = Number.operator("ptr")

# Triggers
NumberStateTrigger = number_ns.class_(
    "NumberStateTrigger", automation.Trigger.template(cg.float_)
)
ValueRangeTrigger = number_ns.class_(
    "ValueRangeTrigger", automation.Trigger.template(cg.float_), cg.Component
)

# Actions
NumberSetAction = number_ns.class_("NumberSetAction", automation.Action)
NumberOperationAction = number_ns.class_("NumberOperationAction", automation.Action)

# Conditions
NumberInRangeCondition = number_ns.class_(
    "NumberInRangeCondition", automation.Condition
)

NumberMode = number_ns.enum("NumberMode")

NUMBER_MODES = {
    "AUTO": NumberMode.NUMBER_MODE_AUTO,
    "BOX": NumberMode.NUMBER_MODE_BOX,
    "SLIDER": NumberMode.NUMBER_MODE_SLIDER,
}

NumberOperation = number_ns.enum("NumberOperation")

NUMBER_OPERATION_OPTIONS = {
    "INCREMENT": NumberOperation.NUMBER_OP_INCREMENT,
    "DECREMENT": NumberOperation.NUMBER_OP_DECREMENT,
    "TO_MIN": NumberOperation.NUMBER_OP_TO_MIN,
    "TO_MAX": NumberOperation.NUMBER_OP_TO_MAX,
}

validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")
validate_unit_of_measurement = cv.string_strict

NUMBER_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTNumberComponent),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(NumberStateTrigger),
                }
            ),
            cv.Optional(CONF_ON_VALUE_RANGE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ValueRangeTrigger),
                    cv.Optional(CONF_ABOVE): cv.templatable(cv.float_),
                    cv.Optional(CONF_BELOW): cv.templatable(cv.float_),
                },
                cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW),
            ),
            cv.Optional(CONF_UNIT_OF_MEASUREMENT): validate_unit_of_measurement,
            cv.Optional(CONF_MODE, default="AUTO"): cv.enum(NUMBER_MODES, upper=True),
            cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
        }
    )
)

_UNDEF = object()


def number_schema(
    class_: MockObjClass,
    *,
    icon: str = _UNDEF,
    entity_category: str = _UNDEF,
    device_class: str = _UNDEF,
    unit_of_measurement: str = _UNDEF,
) -> cv.Schema:
    schema = {cv.GenerateID(): cv.declare_id(class_)}

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
        (CONF_UNIT_OF_MEASUREMENT, unit_of_measurement, validate_unit_of_measurement),
    ]:
        if default is not _UNDEF:
            schema[cv.Optional(key, default=default)] = validator

    return NUMBER_SCHEMA.extend(schema)


async def setup_number_core_(
    var, config, *, min_value: float, max_value: float, step: float
):
    await setup_entity(var, config)

    cg.add(var.traits.set_min_value(min_value))
    cg.add(var.traits.set_max_value(max_value))
    cg.add(var.traits.set_step(step))

    cg.add(var.traits.set_mode(config[CONF_MODE]))

    for conf in config.get(CONF_ON_VALUE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
    for conf in config.get(CONF_ON_VALUE_RANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await cg.register_component(trigger, conf)
        if CONF_ABOVE in conf:
            template_ = await cg.templatable(conf[CONF_ABOVE], [(float, "x")], float)
            cg.add(trigger.set_min(template_))
        if CONF_BELOW in conf:
            template_ = await cg.templatable(conf[CONF_BELOW], [(float, "x")], float)
            cg.add(trigger.set_max(template_))
        await automation.build_automation(trigger, [(float, "x")], conf)

    if (unit_of_measurement := config.get(CONF_UNIT_OF_MEASUREMENT)) is not None:
        cg.add(var.traits.set_unit_of_measurement(unit_of_measurement))
    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.traits.set_device_class(device_class))

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, var, config)


async def register_number(
    var, config, *, min_value: float, max_value: float, step: float
):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_number(var))
    await setup_number_core_(
        var, config, min_value=min_value, max_value=max_value, step=step
    )


async def new_number(config, *args, min_value: float, max_value: float, step: float):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_number(
        var, config, min_value=min_value, max_value=max_value, step=step
    )
    return var


NUMBER_IN_RANGE_CONDITION_SCHEMA = cv.All(
    {
        cv.Required(CONF_ID): cv.use_id(Number),
        cv.Optional(CONF_ABOVE): cv.float_,
        cv.Optional(CONF_BELOW): cv.float_,
    },
    cv.has_at_least_one_key(CONF_ABOVE, CONF_BELOW),
)


@automation.register_condition(
    "number.in_range", NumberInRangeCondition, NUMBER_IN_RANGE_CONDITION_SCHEMA
)
async def number_in_range_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(condition_id, template_arg, paren)

    if (above := config.get(CONF_ABOVE)) is not None:
        cg.add(var.set_min(above))
    if (below := config.get(CONF_BELOW)) is not None:
        cg.add(var.set_max(below))

    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_NUMBER")
    cg.add_global(number_ns.using)


OPERATION_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Number),
    }
)


@automation.register_action(
    "number.set",
    NumberSetAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_VALUE): cv.templatable(cv.float_),
        }
    ),
)
async def number_set_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALUE], args, float)
    cg.add(var.set_value(template_))
    return var


@automation.register_action(
    "number.increment",
    NumberOperationAction,
    automation.maybe_simple_id(
        OPERATION_BASE_SCHEMA.extend(
            {
                cv.Optional(CONF_MODE, default="INCREMENT"): cv.one_of(
                    "INCREMENT", upper=True
                ),
                cv.Optional(CONF_CYCLE, default=True): cv.boolean,
            }
        )
    ),
)
@automation.register_action(
    "number.decrement",
    NumberOperationAction,
    automation.maybe_simple_id(
        OPERATION_BASE_SCHEMA.extend(
            {
                cv.Optional(CONF_MODE, default="DECREMENT"): cv.one_of(
                    "DECREMENT", upper=True
                ),
                cv.Optional(CONF_CYCLE, default=True): cv.boolean,
            }
        )
    ),
)
@automation.register_action(
    "number.to_min",
    NumberOperationAction,
    automation.maybe_simple_id(
        OPERATION_BASE_SCHEMA.extend(
            {
                cv.Optional(CONF_MODE, default="TO_MIN"): cv.one_of(
                    "TO_MIN", upper=True
                ),
            }
        )
    ),
)
@automation.register_action(
    "number.to_max",
    NumberOperationAction,
    automation.maybe_simple_id(
        OPERATION_BASE_SCHEMA.extend(
            {
                cv.Optional(CONF_MODE, default="TO_MAX"): cv.one_of(
                    "TO_MAX", upper=True
                ),
            }
        )
    ),
)
@automation.register_action(
    "number.operation",
    NumberOperationAction,
    OPERATION_BASE_SCHEMA.extend(
        {
            cv.Required(CONF_OPERATION): cv.templatable(
                cv.enum(NUMBER_OPERATION_OPTIONS, upper=True)
            ),
            cv.Optional(CONF_CYCLE, default=True): cv.templatable(cv.boolean),
        }
    ),
)
async def number_to_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if (operation := config.get(CONF_OPERATION)) is not None:
        to_ = await cg.templatable(operation, args, NumberOperation)
        cg.add(var.set_operation(to_))
        if (cycle := config.get(CONF_CYCLE)) is not None:
            template_ = await cg.templatable(cycle, args, bool)
            cg.add(var.set_cycle(template_))
    if (mode := config.get(CONF_MODE)) is not None:
        cg.add(var.set_operation(NUMBER_OPERATION_OPTIONS[mode]))
        if (cycle := config.get(CONF_CYCLE)) is not None:
            cg.add(var.set_cycle(cycle))
    return var
