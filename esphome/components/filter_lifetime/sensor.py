import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DURATION, STATE_CLASS_TOTAL_INCREASING, UNIT_HOUR

from . import filter_lifetime_ns

CONF_MAX_LIFETIME = "max_lifetime"
CONF_IS_ON = "is_on"
CONF_CURRENT_SPEED = "current_speed"
CONF_RUNTIME_HOURS = "runtime_hours"
CONF_REMAINING_DAYS = "remaining_days"

FilterLifetimeSensor = filter_lifetime_ns.class_(
    "FilterLifetime", sensor.Sensor, cg.PollingComponent
)


CONFIG_SCHEMA = (
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
            cv.Required(CONF_IS_ON): cv.returning_lambda,
            cv.Required(CONF_CURRENT_SPEED): cv.returning_lambda,
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
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_max_lifetime(config[CONF_MAX_LIFETIME]))

    is_on_template = await cg.templatable(config[CONF_IS_ON], [], bool)
    cg.add(var.set_is_on(is_on_template))

    current_speed_template = await cg.templatable(config[CONF_CURRENT_SPEED], [], float)
    cg.add(var.set_current_speed(current_speed_template))

    if runtime_hours_config := config.get(CONF_RUNTIME_HOURS):
        sens = await sensor.new_sensor(runtime_hours_config)
        cg.add(var.set_runtime_hours_sensor(sens))

    if remaining_days_config := config.get(CONF_REMAINING_DAYS):
        sens = await sensor.new_sensor(remaining_days_config)
        cg.add(var.set_remaining_days_sensor(sens))
