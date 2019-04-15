import voluptuous as vol

from esphome import pins
from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN
from .. import gpio_ns


GPIOBinarySensor = gpio_ns.class_('GPIOBinarySensor', binary_sensor.BinarySensor, cg.Component)

CONFIG_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOBinarySensor),
    vol.Required(CONF_PIN): pins.gpio_input_pin_schema
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    rhs = GPIOBinarySensor.new(config[CONF_NAME], pin)
    gpio = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(gpio, config)
    yield binary_sensor.register_binary_sensor(gpio, config)

