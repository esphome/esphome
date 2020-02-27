import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import output
from esphome.const import CONF_ID, CONF_MIN_POWER

dimmer_ns = cg.esphome_ns.namespace('dimmer')
Dimmer = dimmer_ns.class_('Dimmer', output.FloatOutput, cg.Component)

CONF_GATE_PIN = 'gate_pin'
CONF_ZERO_CROSS_PIN = 'zero_cross_pin'
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(Dimmer),
    cv.Required(CONF_GATE_PIN): pins.internal_gpio_output_pin_schema,
    cv.Required(CONF_ZERO_CROSS_PIN): pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_MIN_POWER, default=0.1): cv.percentage
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield output.register_output(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_GATE_PIN])
    cg.add(var.set_gate_pin(pin))
    pin = yield cg.gpio_pin_expression(config[CONF_ZERO_CROSS_PIN])
    cg.add(var.set_zero_cross_pin(pin))
    cg.add(var.set_min_power(config[CONF_MIN_POWER]))
