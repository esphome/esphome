"""Grove Ultrasonic Ranger distance sensor.

Uses a shared SIG pin for both trigger and echo pulses,
compatible with the Grove Ultrasonic Ranger (single-pin).
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_SIG_PIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_INPUT,
    CONF_OUTPUT,
    CONF_TIMEOUT,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)

grove_ultrasonic_ns = cg.esphome_ns.namespace("grove_ultrasonic")
GroveUltrasonicSensorComponent = grove_ultrasonic_ns.class_(
    "GroveUltrasonicSensorComponent", sensor.Sensor, cg.PollingComponent
)

# The SIG pin must support both INPUT (echo listening) and OUTPUT (trigger pulse).
_sig_pin_schema = pins.gpio_pin_schema(
    {CONF_INPUT: True, CONF_OUTPUT: True}, internal=True
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        GroveUltrasonicSensorComponent,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_SIG_PIN): _sig_pin_schema,
            cv.Optional(CONF_TIMEOUT, default="3.5m"): cv.distance,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sig = await cg.gpio_pin_expression(config[CONF_SIG_PIN])
    cg.add(var.set_sig_pin(sig))
    cg.add(var.set_timeout_us(config[CONF_TIMEOUT] / (0.000343 / 2)))
