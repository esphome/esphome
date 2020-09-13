from esphome import pins, core
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_PIN, CONF_PERIOD

slow_pwm_ns = cg.esphome_ns.namespace("slow_pwm")
SlowPWMOutput = slow_pwm_ns.class_("SlowPWMOutput", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(SlowPWMOutput),
        cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_PERIOD): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=core.TimePeriod(milliseconds=100)),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield output.register_output(var, config)

    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_period(config[CONF_PERIOD]))
