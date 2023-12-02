import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import MY9231OutputComponent

DEPENDENCIES = ["my9231"]

Channel = MY9231OutputComponent.class_("Channel", output.FloatOutput)

CONF_MY9231_ID = "my9231_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_MY9231_ID): cv.use_id(MY9231OutputComponent),
        cv.Required(CONF_ID): cv.declare_id(Channel),
        cv.Required(CONF_CHANNEL): cv.uint16_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)

    parent = await cg.get_variable(config[CONF_MY9231_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
