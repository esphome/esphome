import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    new_packet_interface,
    packet_interface_schema,
)
from esphome.components.packet_interface.layers import CONF_ADDRESSING
import esphome.config_validation as cv
from esphome.core import HexInt
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CONF_MAX_PAYLOAD_SIZE,
    DOMAIN as ESPNOW_DOMAIN,
    ESPNowComponent,
    espnow_ns,
)

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["espnow"]

CONF_ESPNOW_ID = "espnow_id"
CONF_PEER_ADDRESS = "peer_address"

# The ESP-NOW v1 payload; a larger max_payload_size on the espnow component allows a larger max_packet_size
MAX_PACKET_SIZE = 250
BROADCAST_MAC = "FF:FF:FF:FF:FF:FF"

ESPNowPacketInterface = espnow_ns.class_(
    "ESPNowPacketInterface", packet_interface.PacketInterface
)


def _is_broadcast(config: ConfigType) -> bool:
    return str(config[CONF_PEER_ADDRESS]) == BROADCAST_MAC


def _validate_addressing(config: ConfigType) -> ConfigType:
    if not _is_broadcast(config) and CONF_ADDRESSING in config:
        raise cv.Invalid(
            f"Packets to one '{CONF_PEER_ADDRESS}' are already addressed, so '{CONF_ADDRESSING}' is only "
            "for broadcast",
            path=[CONF_ADDRESSING],
        )
    return config


CONFIG_SCHEMA = cv.All(
    packet_interface_schema(ESPNowPacketInterface, MAX_PACKET_SIZE, 1470).extend(
        {
            cv.GenerateID(CONF_ESPNOW_ID): cv.use_id(ESPNowComponent),
            cv.Optional(CONF_PEER_ADDRESS, default=BROADCAST_MAC): cv.mac_address,
        }
    ),
    _validate_addressing,
)


def _validate_packet_size(config: ConfigType) -> ConfigType:
    payload = fv.full_config.get()[ESPNOW_DOMAIN][CONF_MAX_PAYLOAD_SIZE]
    if config[CONF_MAX_PACKET_SIZE] > payload:
        raise cv.Invalid(
            f"Must not be larger than the espnow {CONF_MAX_PAYLOAD_SIZE} of {payload}",
            path=[CONF_MAX_PACKET_SIZE],
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_packet_size


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_ESPNOW_ID])
    peer = [HexInt(x) for x in config[CONF_PEER_ADDRESS].parts]
    if not _is_broadcast(config):
        cg.add(parent.add_peer(peer))
    await new_packet_interface(config, parent, peer)
