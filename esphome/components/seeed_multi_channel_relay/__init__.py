from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS
from esphome.types import ConfigType

CODEOWNERS = ["@KoenBreeman"]
DEPENDENCIES = ["i2c"]
DOMAIN = "seeed_multi_channel_relay"

MULTI_CONF = True

CONF_SEEED_MULTI_CHANNEL_RELAY_ID = "seeed_multi_channel_relay_id"

seeed_multi_channel_relay_ns = cg.esphome_ns.namespace("seeed_multi_channel_relay")
SeeedMultiChannelRelay = seeed_multi_channel_relay_ns.class_(
    "SeeedMultiChannelRelay", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SeeedMultiChannelRelay),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x11))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)


automation.register_apply_action(
    "seeed_multi_channel_relay.change_address",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(SeeedMultiChannelRelay),
            cv.Required(CONF_ADDRESS): cv.templatable(
                cv.hex_int_range(min=0x08, max=0x77)
            ),
        },
        key=CONF_ADDRESS,
    ),
    automation.ApplyField(CONF_ADDRESS, "change_address", cg.uint8),
)
