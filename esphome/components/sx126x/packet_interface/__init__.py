from esphome.components import packet_interface
from esphome.components.packet_interface.radio_link import (
    new_radio_link,
    radio_link_final_validate,
    radio_link_schema,
)
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    CONF_MODULATION,
    CONF_PAYLOAD_LENGTH,
    CONF_RX_START,
    CONF_SX126X_ID,
    CONF_SYNC_VALUE,
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


def _check_fsk(config: ConfigType, radio: ConfigType) -> None:
    """Without LoRa, the radio can tell that a packet is arriving only from its sync word, which listen before talk
    needs.
    """
    if radio[CONF_MODULATION] != "LORA" and not radio[CONF_SYNC_VALUE]:
        raise cv.Invalid(
            f"Without LoRa modulation the radio needs a '{CONF_SYNC_VALUE}', so that the link can tell when a packet "
            "is arriving and not send over it",
            path=[CONF_SX126X_ID],
        )


FINAL_VALIDATE_SCHEMA = radio_link_final_validate(
    CONF_SX126X_ID, CONF_PAYLOAD_LENGTH, CONF_RX_START, _check_fsk
)


async def to_code(config: ConfigType) -> None:
    await new_radio_link(config, CONF_SX126X_ID, DOMAIN)
