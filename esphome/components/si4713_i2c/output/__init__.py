import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_PIN
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
)

BinaryOutput = si4713_ns.class_("BinaryOutput", output.BinaryOutput, cg.Component)

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BinaryOutput),
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
        cv.Required(CONF_PIN): cv.int_range(min=1, max=3),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SI4713_ID])
    cg.add(var.set_pin(config[CONF_PIN]))
