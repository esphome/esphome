import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import output
from esphome.const import CONF_ID, CONF_PIN
from .. import gpio_ns

GPIOBinaryOutput = gpio_ns.class_('GPIOBinaryOutput', output.BinaryOutput,
                                  cg.Component)

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_variable_id(GPIOBinaryOutput),
    cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    gpio = cg.new_Pvariable(config[CONF_ID], pin)
    yield output.register_output(gpio, config)
    yield cg.register_component(gpio, config)
