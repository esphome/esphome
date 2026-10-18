import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    new_packet_interface,
    packet_interface_schema,
)
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_PAYLOAD_LENGTH, CONF_SX126X_ID, SX126x, SX126xListener, sx126x_ns

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["sx126x"]

# The radio's payload length register is 8 bits
MAX_PACKET_SIZE = 255

SX126xPacketInterface = sx126x_ns.class_(
    "SX126xPacketInterface", packet_interface.PacketInterface, SX126xListener
)

CONFIG_SCHEMA = packet_interface_schema(
    SX126xPacketInterface, MAX_PACKET_SIZE, MAX_PACKET_SIZE
).extend({cv.GenerateID(CONF_SX126X_ID): cv.use_id(SX126x)})


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    path = full_config.get_path_for_id(config[CONF_SX126X_ID])[:-1]
    if full_config.get_config_for_path(path)[CONF_PAYLOAD_LENGTH] != 0:
        raise cv.Invalid(
            f"Packets vary in length, so the radio's '{CONF_PAYLOAD_LENGTH}' must be 0",
            path=[CONF_SX126X_ID],
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    radio = await cg.get_variable(config[CONF_SX126X_ID])
    await new_packet_interface(config, radio)
