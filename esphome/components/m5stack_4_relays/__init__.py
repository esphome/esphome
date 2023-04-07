import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@Koen Breeman"]
DEPENDENCIES = ["i2c"]

MULTI_CONF = True

CONF_I2C_ADDR = 0x26

CONF_M5Stack_4_Relays_ID = "m5stack_4_relays_id"

M5Stack_ns = cg.esphome_ns.namespace("m5stack_4_relays")
M5Stack_4_Relays = M5Stack_ns.class_("M5Stack_4_Relays", cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Stack_4_Relays),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(CONF_I2C_ADDR))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
