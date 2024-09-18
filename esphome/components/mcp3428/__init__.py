import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@mdop"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

mcp3428_ns = cg.esphome_ns.namespace("mcp3428")
MCP3428Component = mcp3428_ns.class_("MCP3428Component", cg.Component, i2c.I2CDevice)

CONF_CONTINUOUS_MODE = "continuous_mode"
CONF_MCP3428_ID = "mcp3428_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP3428Component),
            cv.Optional(CONF_CONTINUOUS_MODE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x68))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_continuous_mode(config[CONF_CONTINUOUS_MODE]))
