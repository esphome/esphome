import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import output, i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]

mcp47a1_ns = cg.esphome_ns.namespace("mcp47a1")
MCP47A1 = mcp47a1_ns.class_("MCP47A1", output.FloatOutput, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(MCP47A1),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x2E))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await output.register_output(var, config)
