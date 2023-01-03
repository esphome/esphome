from esphome import pins, automation
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_NUMBER,
    CONF_PIN,
)

DEPENDENCIES = ["esp8266"]


def valid_pwm_pin(value):
    num = value[CONF_NUMBER]
    cv.one_of(0, 1, 2, 3, 4, 5, 9, 10, 12, 13, 14, 15, 16)(num)
    return value


esp8266_pwm_ns = cg.esphome_ns.namespace("esp8266_pwm")
ESP8266PWM = esp8266_pwm_ns.class_("ESP8266PWM", output.FloatOutput, cg.Component)
SetFrequencyAction = esp8266_pwm_ns.class_("SetFrequencyAction", automation.Action)
validate_frequency = cv.All(cv.frequency, cv.Range(min=1.0e-6))

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(ESP8266PWM),
            cv.Required(CONF_PIN): cv.All(
                pins.internal_gpio_output_pin_schema, valid_pwm_pin
            ),
            cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 4, 0),
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_frequency(config[CONF_FREQUENCY]))


@automation.register_action(
    "output.esp8266_pwm.set_frequency",
    SetFrequencyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(ESP8266PWM),
            cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
        }
    ),
)
async def esp8266_set_frequency_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_FREQUENCY], args, float)
    cg.add(var.set_frequency(template_))
    return var
