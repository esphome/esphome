import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_EMC2303_ID, CONF_FAN, Emc2303Component, emc2303_ns

DEPENDENCIES = ["emc2303"]

Emc2303Output = emc2303_ns.class_("Emc2303Output", output.FloatOutput)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Emc2303Output),
        cv.GenerateID(CONF_EMC2303_ID): cv.use_id(Emc2303Component),
        cv.Required(CONF_FAN): cv.int_range(min=1, max=3),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_EMC2303_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    cg.add(var.set_fan(config[CONF_FAN]))
    await output.register_output(var, config)
