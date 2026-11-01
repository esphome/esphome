from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DURATION, STATE_CLASS_TOTAL_INCREASING, UNIT_HOUR
from esphome.types import ConfigType

CONF_MAX_LIFETIME = "max_lifetime"
CONF_IS_ON = "is_on"
CONF_IS_ON_SENSOR = "is_on_sensor"
CONF_CURRENT_SPEED = "current_speed"
CONF_CURRENT_SPEED_SENSOR = "current_speed_sensor"
CONF_RUNTIME_HOURS = "runtime_hours"
CONF_REMAINING_DAYS = "remaining_days"
UNIT_DAY = "d"

filter_lifetime_ns = cg.esphome_ns.namespace("filter_lifetime")
FilterLifetime = filter_lifetime_ns.class_(
    "FilterLifetime", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        FilterLifetime, kind=sensor.PERCENT, accuracy_decimals=2, icon="mdi:air-filter"
    )
    .extend(
        {
            cv.Required(CONF_MAX_LIFETIME): cv.All(
                cv.positive_time_period_minutes,
                cv.Range(min=cv.TimePeriod(minutes=1)),
            ),
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
                unit_of_measurement=UNIT_DAY,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DURATION,
                icon="mdi:calendar-clock",
            ),
        }
    )
    .extend(cv.polling_component_schema("60s")),
    cv.has_at_most_one_key(CONF_IS_ON, CONF_IS_ON_SENSOR),
    cv.has_at_most_one_key(CONF_CURRENT_SPEED, CONF_CURRENT_SPEED_SENSOR),
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_max_lifetime(config[CONF_MAX_LIFETIME]))

    if (is_on := config.get(CONF_IS_ON)) is not None:
        cg.add(var.set_is_on_lambda(await cg.templatable(is_on, [], bool)))
    await cg.add_variable(var.set_is_on_binary_sensor, config.get(CONF_IS_ON_SENSOR))

    if (current_speed := config.get(CONF_CURRENT_SPEED)) is not None:
        cg.add(
            var.set_current_speed_lambda(await cg.templatable(current_speed, [], float))
        )
    await cg.add_variable(
        var.set_current_speed_sensor, config.get(CONF_CURRENT_SPEED_SENSOR)
    )

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RUNTIME_HOURS, var.set_runtime_hours_sensor)
    await sensors(CONF_REMAINING_DAYS, var.set_remaining_days_sensor)


automation.register_apply_action(
    "filter_lifetime.reset_filter",
    cv.Schema({cv.GenerateID(): cv.use_id(FilterLifetime)}),
    automation.ApplyCall("reset_filter()"),
)
