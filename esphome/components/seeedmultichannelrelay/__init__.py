import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@KoenBreeman"]
DEPENDENCIES = ["i2c"]

MULTI_CONF = True

CONF_I2C_ADDR = 0x11

CONF_SEEEDMULTICHANNELRELAY_ID = "seeedmultichannelrelay_id"
CONF_CHANGE_ADDRESS_TO = "change_address_to"

seeedmultichannelrelay_ns = cg.esphome_ns.namespace("seeedmultichannelrelay")
SeeedMultiChannelRelay = seeedmultichannelrelay_ns.class_(
    "SeeedMultiChannelRelay", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SeeedMultiChannelRelay),
            cv.Optional(CONF_CHANGE_ADDRESS_TO): cv.hex_int_range(min=0x00, max=0x7F),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(CONF_I2C_ADDR))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if CONF_CHANGE_ADDRESS_TO in config:
        cg.add(var.change_i2c_address(config[CONF_CHANGE_ADDRESS_TO]))
