import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_PIN
from .. import gpio_ns

GPIOBinarySensor = gpio_ns.class_('GPIOBinarySensor', binary_sensor.BinarySensor, cg.Component)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(GPIOBinarySensor),
    cv.Required(CONF_PIN): pins.gpio_input_pin_schema
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
