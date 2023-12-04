import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@berfenger"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
CONF_STORE_IN_EEPROM = "store_in_eeprom"

mcp4728_ns = cg.esphome_ns.namespace("mcp4728")
MCP4728Component = mcp4728_ns.class_("MCP4728Component", cg.Component, i2c.I2CDevice)
CONF_MCP4728_ID = "mcp4728_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MCP4728Component),
            cv.Optional(CONF_STORE_IN_EEPROM, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x60))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_STORE_IN_EEPROM])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
