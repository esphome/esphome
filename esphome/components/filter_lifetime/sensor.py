import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DURATION, STATE_CLASS_TOTAL_INCREASING, UNIT_HOUR

from . import filter_lifetime_ns

CONF_MAX_LIFETIME = "max_lifetime"
CONF_IS_ON = "is_on"
CONF_IS_ON_SENSOR = "is_on_sensor"
CONF_CURRENT_SPEED = "current_speed"
CONF_CURRENT_SPEED_SENSOR = "current_speed_sensor"
CONF_RUNTIME_HOURS = "runtime_hours"
CONF_REMAINING_DAYS = "remaining_days"

FilterLifetimeSensor = filter_lifetime_ns.class_(
    "FilterLifetime", sensor.Sensor, cg.PollingComponent
)


def validate_is_on_config(config):
    """Validate that is_on lambda and is_on_sensor are not both provided."""
    has_lambda = CONF_IS_ON in config
    has_sensor = CONF_IS_ON_SENSOR in config
    if has_lambda and has_sensor:
        raise cv.Invalid(
            "Only one of 'is_on' (lambda) or 'is_on_sensor' can be provided, not both"
        )
    return config


def validate_speed_config(config):
    """Validate that current_speed lambda and current_speed_sensor are not both provided."""
    has_lambda = CONF_CURRENT_SPEED in config
    has_sensor = CONF_CURRENT_SPEED_SENSOR in config
    if has_lambda and has_sensor:
        raise cv.Invalid(
            "Only one of 'current_speed' (lambda) or 'current_speed_sensor' can be provided, not both"
        )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        FilterLifetimeSensor,
        unit_of_measurement="%",
        accuracy_decimals=2,
        icon="mdi:air-filter",
        state_class="measurement",
    )
    .extend(
        {
            cv.Required(CONF_MAX_LIFETIME): cv.positive_int,
            cv.Optional(CONF_IS_ON): cv.returning_lambda,
            cv.Optional(CONF_IS_ON_SENSOR): cv.use_id(binary_sensor.BinarySensor),
            cv.Optional(CONF_CURRENT_SPEED): cv.returning_lambda,
            cv.Optional(CONF_CURRENT_SPEED_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_RUNTIME_HOURS): sensor.sensor_schema(
                unit_of_measurement=UNIT_HOUR,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_DURATION,
                state_class=STATE_CLASS_TOTAL_INCREASING,
                icon="mdi:timer-outline",
            ),
            cv.Optional(CONF_REMAINING_DAYS): sensor.sensor_schema(
                unit_of_measurement="d",
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

    # Handle is_on: either lambda or binary_sensor
    if CONF_IS_ON in config:
        is_on_template = await cg.templatable(config[CONF_IS_ON], [], bool)
        cg.add(var.set_is_on_lambda(is_on_template))
    elif CONF_IS_ON_SENSOR in config:
        sens = await cg.get_variable(config[CONF_IS_ON_SENSOR])
        cg.add(var.set_is_on_sensor(sens))

    # Handle current_speed: either lambda or sensor
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
