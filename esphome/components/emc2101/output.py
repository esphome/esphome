import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID
from . import Emc2101Component, emc2101_ns

DEPENDENCIES = ["emc2101"]

EMC2101Output = emc2101_ns.class_("EMC2101Output", output.FloatOutput, cg.Component)
CONF_EMC2101_ID = "emc2101_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(EMC2101Output),
        cv.GenerateID(CONF_EMC2101_ID): cv.use_id(Emc2101Component),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_EMC2101_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    await output.register_output(var, config)
