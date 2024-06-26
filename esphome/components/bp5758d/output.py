import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_CURRENT
from . import BP5758D

DEPENDENCIES = ["bp5758d"]

Channel = BP5758D.class_("Channel", output.FloatOutput)

CONF_BP5758D_ID = "bp5758d_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BP5758D_ID): cv.use_id(BP5758D),
        cv.Required(CONF_ID): cv.declare_id(Channel),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=5),
        cv.Optional(CONF_CURRENT, default=10): cv.int_range(min=0, max=90),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_BP5758D_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_current(config[CONF_CURRENT]))
