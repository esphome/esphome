import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

CODEOWNERS = ["@KoenBreeman"]
DEPENDENCIES = ["i2c"]
DOMAIN = "seeed_multi_channel_relay"

MULTI_CONF = True

CONF_I2C_ADDR = 0x11

CONF_SEEED_MULTI_CHANNEL_RELAY_ID = "seeed_multi_channel_relay_id"
CONF_CHANGE_ADDRESS_TO = "change_address_to"

seeed_multi_channel_relay_ns = cg.esphome_ns.namespace("seeed_multi_channel_relay")
Seeed_Multi_Channel_Relay = seeed_multi_channel_relay_ns.class_(
    "SeeedMultiChannelRelay", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Seeed_Multi_Channel_Relay),
            cv.Optional(CONF_CHANGE_ADDRESS_TO): cv.hex_int_range(min=0x00, max=0x7F),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(CONF_I2C_ADDR))
)


async def to_code(config):
    var = await i2c.new_i2c_component(config)
    if CONF_CHANGE_ADDRESS_TO in config:
        cg.add(var.change_i2c_address(config[CONF_CHANGE_ADDRESS_TO]))
