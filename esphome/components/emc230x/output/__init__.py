import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_EMC230X_ID, CONF_FAN, Emc230xComponent, emc230x_ns

DEPENDENCIES = ["emc230x"]

Emc230xOutput = emc230x_ns.class_("Emc230xOutput", output.FloatOutput)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Emc230xOutput),
        cv.GenerateID(CONF_EMC230X_ID): cv.use_id(Emc230xComponent),
        cv.Required(CONF_FAN): cv.int_range(min=1, max=5),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_EMC230X_ID])
    var = cg.new_Pvariable(config[CONF_ID], paren)
    cg.add(var.set_fan(config[CONF_FAN]))
    await output.register_output(var, config)
