from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_PIN

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["rp2040"]


rp2040_pwm_ns = cg.esphome_ns.namespace("rp2040_pwm")
RP2040PWM = rp2040_pwm_ns.class_("RP2040PWM", output.FloatOutput, cg.Component)
SetFrequencyAction = rp2040_pwm_ns.class_("SetFrequencyAction", automation.Action)
validate_frequency = cv.All(cv.frequency, cv.Range(min=1.0e-6))

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(RP2040PWM),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): validate_frequency,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))

    cg.add(var.set_frequency(config[CONF_FREQUENCY]))


@automation.register_action(
    "output.rp2040_pwm.set_frequency",
    SetFrequencyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RP2040PWM),
            cv.Required(CONF_FREQUENCY): cv.templatable(validate_frequency),
        }
    ),
)
async def rp2040_set_frequency_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_FREQUENCY], args, float)
    cg.add(var.set_frequency(template_))
    return var
