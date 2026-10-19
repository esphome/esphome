from esphome.components import packet_interface
from esphome.components.packet_interface.radio_link import (
    new_radio_link,
    radio_link_final_validate,
    radio_link_schema,
)
from esphome.types import ConfigType

from .. import (
    CONF_PAYLOAD_LENGTH,
    CONF_SX126X_ID,
    DOMAIN,
    SX126x,
    SX126xListener,
    sx126x_ns,
)

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["sx126x"]

SX126xPacketInterface = sx126x_ns.class_(
    "SX126xPacketInterface", packet_interface.PacketInterface, SX126xListener
)

CONFIG_SCHEMA = radio_link_schema(SX126xPacketInterface, CONF_SX126X_ID, SX126x)

FINAL_VALIDATE_SCHEMA = radio_link_final_validate(CONF_SX126X_ID, CONF_PAYLOAD_LENGTH)


async def to_code(config: ConfigType) -> None:
    await new_radio_link(config, CONF_SX126X_ID, DOMAIN)
