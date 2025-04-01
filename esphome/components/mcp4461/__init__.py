import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@p1ngb4ck"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True
CONF_DISABLE_WIPER_0 = "disable_wiper_0"
CONF_DISABLE_WIPER_1 = "disable_wiper_1"
CONF_DISABLE_WIPER_2 = "disable_wiper_2"
CONF_DISABLE_WIPER_3 = "disable_wiper_3"

mcp4461_ns = cg.esphome_ns.namespace("mcp4461")
Mcp4461Component = mcp4461_ns.class_("Mcp4461Component", cg.Component, i2c.I2CDevice)
CONF_MCP4461_ID = "mcp4461_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Mcp4461Component),
            cv.Optional(CONF_DISABLE_WIPER_0, default=False): cv.boolean,
            cv.Optional(CONF_DISABLE_WIPER_1, default=False): cv.boolean,
            cv.Optional(CONF_DISABLE_WIPER_2, default=False): cv.boolean,
            cv.Optional(CONF_DISABLE_WIPER_3, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x2C))
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_DISABLE_WIPER_0],
        config[CONF_DISABLE_WIPER_1],
        config[CONF_DISABLE_WIPER_2],
        config[CONF_DISABLE_WIPER_3],
    )
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
