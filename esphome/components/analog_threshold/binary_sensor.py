import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core
from esphome.components import binary_sensor, sensor
from esphome.const import (
    CONF_DELAY,
    CONF_HIGH,
    CONF_ID,
    CONF_LOW,
    CONF_SENSOR_ID,
    CONF_THRESHOLD,
    CONF_INVERTED,
)

analog_threshold_ns = cg.esphome_ns.namespace("analog_threshold")

AnalogThresholdBinarySensor = analog_threshold_ns.class_(
    "AnalogThresholdBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONF_UPPER = "upper"
CONF_LOWER = "lower"

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AnalogThresholdBinarySensor),
        cv.Required(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
        cv.Required(CONF_THRESHOLD): cv.Any(
            cv.float_,
            cv.Schema(
                {cv.Required(CONF_UPPER): cv.float_, cv.Required(CONF_LOWER): cv.float_}
            ),
        ),
        cv.Optional(CONF_INVERTED, False): cv.boolean,
        cv.Optional(CONF_DELAY, "0s"): cv.Any(
            cv.positive_time_period_milliseconds,
            cv.Schema(
                {
                    cv.Required(CONF_HIGH): cv.positive_time_period_milliseconds,
                    cv.Required(CONF_LOW): cv.positive_time_period_milliseconds,
                }
            ),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    bin = await cg.get_variable(config[CONF_SENSOR_ID])
    cg.add(var.set_sensor(bin))

    if isinstance(config[CONF_THRESHOLD], float):
        cg.add(var.set_upper_threshold(config[CONF_THRESHOLD]))
        cg.add(var.set_lower_threshold(config[CONF_THRESHOLD]))
    else:
        cg.add(var.set_upper_threshold(config[CONF_THRESHOLD][CONF_UPPER]))
        cg.add(var.set_lower_threshold(config[CONF_THRESHOLD][CONF_LOWER]))

    cg.add(var.set_inverted(config[CONF_INVERTED]))

    if isinstance(config[CONF_DELAY], core.TimePeriodMilliseconds):
        cg.add(var.set_delay_high(config[CONF_DELAY]))
        cg.add(var.set_delay_low(config[CONF_DELAY]))
    else:
        cg.add(var.set_delay_high(config[CONF_DELAY][CONF_HIGH]))
        cg.add(var.set_delay_low(config[CONF_DELAY][CONF_LOW]))
