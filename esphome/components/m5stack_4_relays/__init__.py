import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@Koen Breeman"]
DEPENDENCIES = ['i2c']

MULTI_CONF = True

CONF_I2C_ADDR = 0X26

CONF_M5Stack_4_Relays_ID = "M5Stack_4_Relays_id"

#CONF_SYNC_MODE = "sync_mode"

M5Stack_ns = cg.esphome_ns.namespace("M5Stack_4_Relays")
M5Stack_4_Relays = M5Stack_ns.class_("M5Stack_4_Relays",  cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (cv.Schema({
            cv.GenerateID(): cv.declare_id(M5Stack_4_Relays),
            #cv.Optional(CONF_SYNC_MODE, default= True): cv.boolean, 
        })
        .extend(cv.COMPONENT_SCHEMA)
        .extend(i2c.i2c_device_schema(CONF_I2C_ADDR)))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    #cg.add(var.set_switchMode(config[CONF_SYNC_MODE]))

