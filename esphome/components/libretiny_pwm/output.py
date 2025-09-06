from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_PIN

DEPENDENCIES = ["libretiny"]

libretinypwm_ns = cg.esphome_ns.namespace("libretiny_pwm")
LibreTinyPWM = libretinypwm_ns.class_("LibreTinyPWM", output.FloatOutput, cg.Component)
SetFrequencyAction = libretinypwm_ns.class_("SetFrequencyAction", automation.Action)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(LibreTinyPWM),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Optional(CONF_FREQUENCY, default="1kHz"): cv.frequency,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    gpio = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], gpio)
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(var.set_frequency(config[CONF_FREQUENCY]))


@automation.register_action(
    "output.libretiny_pwm.set_frequency",
    SetFrequencyAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(LibreTinyPWM),
            cv.Required(CONF_FREQUENCY): cv.templatable(cv.int_),
        }
    ),
)
async def libretiny_pwm_set_frequency_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_FREQUENCY], args, float)
    cg.add(var.set_frequency(template_))
    return var
