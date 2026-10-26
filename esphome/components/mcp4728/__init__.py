import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@berfenger"]
DEPENDENCIES = ["i2c"]
DOMAIN = "mcp4728"
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


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config, config[CONF_STORE_IN_EEPROM])
