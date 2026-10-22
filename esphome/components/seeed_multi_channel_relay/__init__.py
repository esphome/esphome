import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@KoenBreeman"]
DEPENDENCIES = ["i2c"]
DOMAIN = "seeed_multi_channel_relay"

MULTI_CONF = True

CONF_CHANGE_ADDRESS_TO = "change_address_to"
CONF_SEEED_MULTI_CHANNEL_RELAY_ID = "seeed_multi_channel_relay_id"

seeed_multi_channel_relay_ns = cg.esphome_ns.namespace("seeed_multi_channel_relay")
SeeedMultiChannelRelay = seeed_multi_channel_relay_ns.class_(
    "SeeedMultiChannelRelay", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SeeedMultiChannelRelay),
            cv.Optional(CONF_CHANGE_ADDRESS_TO): cv.hex_int_range(min=0x08, max=0x77),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x11))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    cg.add_optional(var.set_change_address_to, config.get(CONF_CHANGE_ADDRESS_TO))
