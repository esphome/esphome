import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_PIN,
    STATE_CLASS_MEASUREMENT,
    UNIT_SECOND,
    ICON_TIMER,
)

pulse_width_ns = cg.esphome_ns.namespace("pulse_width")

PulseWidthSensor = pulse_width_ns.class_(
    "PulseWidthSensor", sensor.Sensor, cg.PollingComponent
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_SECOND,
        icon=ICON_TIMER,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(PulseWidthSensor),
            cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
