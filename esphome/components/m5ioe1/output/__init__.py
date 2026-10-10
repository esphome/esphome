import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from .. import BASE_SCHEMA, CONF_M5IOE1_ID, PWM_CHANNEL, PWM_POLARITY, m5ioe1_ns

DEPENDENCIES = ["m5ioe1"]

M5IOE1FloatOutput = m5ioe1_ns.class_(
    "M5IOE1FloatOutput", output.FloatOutput, cg.Component
)

CONF_PWM_POLARITY = "pwm_polarity"


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(M5IOE1FloatOutput),
        cv.Required(CONF_CHANNEL): cv.enum(PWM_CHANNEL),
        cv.Optional(CONF_PWM_POLARITY, default="ACTIVE_HIGH"): cv.enum(PWM_POLARITY),
    }
).extend(BASE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5IOE1_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(var.set_channel(config[CONF_CHANNEL]))

    if CONF_PWM_POLARITY in config:
        cg.add(var.set_pwm_polarity(config[CONF_PWM_POLARITY]))
