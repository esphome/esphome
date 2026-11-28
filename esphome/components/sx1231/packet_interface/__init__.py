from esphome.components import packet_interface
from esphome.components.packet_interface.radio_link import (
    new_radio_link,
    radio_link_final_validate,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    CONF_PACKET_MODE,
    CONF_PAYLOAD_LENGTH,
    CONF_RX_START,
    CONF_SX1231_ID,
    CONF_SYNC_VALUE,
    MAX_PACKET_SIZE,
    SX1231,
    SX1231Listener,
    sx1231_ns,
)

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["sx1231"]

SX1231PacketInterface = sx1231_ns.class_(
    "SX1231PacketInterface", packet_interface.PacketInterface, SX1231Listener
)

# radio_link_schema() would default to 255 bytes, more than the radio sends
CONFIG_SCHEMA = packet_interface.packet_interface_schema(
    SX1231PacketInterface,
    MAX_PACKET_SIZE,
    MAX_PACKET_SIZE,
    chooses_reliability_timeout=True,
).extend({cv.GenerateID(CONF_SX1231_ID): cv.use_id(SX1231)})


def _check(config: ConfigType, radio: ConfigType) -> None:
    """The radio must send packets, not a continuous bit stream.

    It can tell that a packet is arriving only from its sync word, which listen before talk needs.
    """
    if not radio[CONF_PACKET_MODE]:
        raise cv.Invalid(
            f"The radio's '{CONF_PACKET_MODE}' must be true, as a link sends packets",
            path=[CONF_SX1231_ID],
        )
    if not radio[CONF_SYNC_VALUE]:
        raise cv.Invalid(
            f"The radio needs a '{CONF_SYNC_VALUE}', so that the link can tell when a packet is arriving and "
            "not send over it",
            path=[CONF_SX1231_ID],
        )


def _receive_limit(radio: ConfigType) -> int:
    return MAX_PACKET_SIZE


FINAL_VALIDATE_SCHEMA = radio_link_final_validate(
    CONF_SX1231_ID, CONF_PAYLOAD_LENGTH, CONF_RX_START, _check, _receive_limit
)


async def to_code(config: ConfigType) -> None:
    await new_radio_link(config, CONF_SX1231_ID, _receive_limit)
