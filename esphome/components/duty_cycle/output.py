from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (CONF_FREQUENCY, CONF_ID, CONF_NUMBER, CONF_PIN, ESP_PLATFORM_ESP8266,
    CONF_PERIOD)

duty_cycle_ns = cg.esphome_ns.namespace('duty_cycle')
DutyCycleOutput = duty_cycle_ns.class_('DutyCycleOutput', output.FloatOutput, cg.Component)
validate_frequency = cv.All(cv.frequency, cv.Range(min=1.0e-6))

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_id(DutyCycleOutput),
    cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_PERIOD): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield output.register_output(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_period(config[CONF_PERIOD]))
