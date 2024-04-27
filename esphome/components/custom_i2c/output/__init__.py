from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_BYTES

from ... import custom_i2c
from ..constants import CONF_REGISTER_ID

CustomI2COutput = custom_i2c.custom_i2c_ns.class_(
    "CustomI2COutput", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(CustomI2COutput),
        cv.Required(CONF_REGISTER_ID): cv.use_id(custom_i2c.CustomI2CRegister),
        cv.Optional(CONF_BYTES, 1): cv.int_range(1, 4),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], cg.TemplateArguments(config[CONF_BYTES]))
    await cg.register_component(var, config)
    await output.register_output(var, config)

    cg.add(var.set_register(await cg.get_variable(config[CONF_REGISTER_ID])))
