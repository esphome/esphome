from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_PIN, ICON_TIMER, STATE_CLASS_MEASUREMENT, UNIT_SECOND

pulse_width_ns = cg.esphome_ns.namespace("pulse_width")

PulseWidthSensor = pulse_width_ns.class_(
    "PulseWidthSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        PulseWidthSensor,
        unit_of_measurement=UNIT_SECOND,
        icon=ICON_TIMER,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
