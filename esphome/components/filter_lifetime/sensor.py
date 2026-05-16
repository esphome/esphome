from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL,
    UNIT_DAY,
    UNIT_HOUR,
    UNIT_PERCENT,
)

CONF_MAX_LIFETIME = "max_lifetime"
CONF_IS_ON = "is_on"
CONF_IS_ON_SENSOR = "is_on_sensor"
CONF_CURRENT_SPEED = "current_speed"
CONF_CURRENT_SPEED_SENSOR = "current_speed_sensor"
CONF_RUNTIME_HOURS = "runtime_hours"
CONF_REMAINING_DAYS = "remaining_days"

filter_lifetime_ns = cg.esphome_ns.namespace("filter_lifetime")
FilterLifetime = filter_lifetime_ns.class_(
    "FilterLifetime", sensor.Sensor, cg.PollingComponent
)
ResetFilterAction = filter_lifetime_ns.class_("ResetFilterAction", automation.Action)


def validate_is_on_config(config):
    has_lambda = CONF_IS_ON in config
    has_sensor = CONF_IS_ON_SENSOR in config
    if has_lambda and has_sensor:
        raise cv.Invalid(
            "Only one of 'is_on' (lambda) or 'is_on_sensor' can be provided, not both"
        )
    return config


def validate_speed_config(config):
    has_lambda = CONF_CURRENT_SPEED in config
    has_sensor = CONF_CURRENT_SPEED_SENSOR in config
    if has_lambda and has_sensor:
        raise cv.Invalid(
            "Only one of 'current_speed' (lambda) or 'current_speed_sensor' can be provided, not both"
        )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        FilterLifetime,
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=2,
        icon="mdi:air-filter",
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_MAX_LIFETIME): cv.int_range(min=1),
            cv.Optional(CONF_IS_ON): cv.returning_lambda,
            cv.Optional(CONF_IS_ON_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_CURRENT_SPEED): cv.returning_lambda,
            cv.Optional(CONF_CURRENT_SPEED_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_RUNTIME_HOURS): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_TOTAL,
                icon="mdi:timer-outline",
            ),
            cv.Optional(CONF_REMAINING_DAYS): sensor.sensor_schema(
                unit_of_measurement=UNIT_DAY,
                accuracy_decimals=0,
                icon="mdi:calendar-clock",
            ),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    validate_is_on_config,
    validate_speed_config,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_max_lifetime(config[CONF_MAX_LIFETIME]))

    if CONF_IS_ON in config:
        is_on_template = await cg.templatable(config[CONF_IS_ON], [], bool)
        cg.add(var.set_is_on_lambda(is_on_template))
    elif CONF_IS_ON_SENSOR in config:
        sens = await cg.get_variable(config[CONF_IS_ON_SENSOR])
        cg.add(var.set_is_on_sensor(sens))

    if CONF_CURRENT_SPEED in config:
        current_speed_template = await cg.templatable(
            config[CONF_CURRENT_SPEED], [], float
        )
        cg.add(var.set_current_speed_lambda(current_speed_template))
    elif CONF_CURRENT_SPEED_SENSOR in config:
        sens = await cg.get_variable(config[CONF_CURRENT_SPEED_SENSOR])
        cg.add(var.set_current_speed_sensor(sens))

    if runtime_hours_config := config.get(CONF_RUNTIME_HOURS):
        sens = await sensor.new_sensor(runtime_hours_config)
        cg.add(var.set_runtime_hours_sensor(sens))

    if remaining_days_config := config.get(CONF_REMAINING_DAYS):
        sens = await sensor.new_sensor(remaining_days_config)
        cg.add(var.set_remaining_days_sensor(sens))


@automation.register_action(
    "filter_lifetime.reset_filter",
    ResetFilterAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(FilterLifetime),
        }
    ),
    synchronous=True,
)
async def reset_filter_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
