from esphome.components import packet_interface
from esphome.components.packet_interface import CONF_MAX_PACKET_SIZE
from esphome.components.packet_interface.radio_link import (
    new_radio_link,
    radio_link_final_validate,
    radio_link_schema,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    CONF_MODULATION,
    CONF_PACKET_MODE,
    CONF_PAYLOAD_LENGTH,
    CONF_RX_START,
    CONF_SX127X_ID,
    DOMAIN,
    SX127x,
    SX127xListener,
    sx127x_ns,
)

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["sx127x"]

# The FIFO limit outside LoRa mode
FSK_MAX_PACKET_SIZE = 64

SX127xPacketInterface = sx127x_ns.class_(
    "SX127xPacketInterface", packet_interface.PacketInterface, SX127xListener
)

CONFIG_SCHEMA = radio_link_schema(SX127xPacketInterface, CONF_SX127X_ID, SX127x)


def _check_fsk(config: ConfigType, radio: ConfigType) -> None:
    """Without LoRa, the radio must send packets, not a continuous bit stream, and holds fewer bytes."""
    if radio[CONF_MODULATION] == "LORA":
        return
    if not radio.get(CONF_PACKET_MODE, False):
        raise cv.Invalid(
            f"Without LoRa modulation the radio's '{CONF_PACKET_MODE}' must be true, as a link sends packets",
            path=[CONF_SX127X_ID],
        )
    if config[CONF_MAX_PACKET_SIZE] > FSK_MAX_PACKET_SIZE:
        raise cv.Invalid(
            f"Without LoRa modulation the radio sends at most {FSK_MAX_PACKET_SIZE} bytes",
            path=[CONF_MAX_PACKET_SIZE],
        )


FINAL_VALIDATE_SCHEMA = radio_link_final_validate(
    CONF_SX127X_ID, CONF_PAYLOAD_LENGTH, CONF_RX_START, _check_fsk
)


async def to_code(config: ConfigType) -> None:
    await new_radio_link(config, CONF_SX127X_ID, DOMAIN)
