import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    CONF_MAX_PACKET_SIZE,
    new_packet_interface,
    packet_interface_schema,
)
from esphome.components.packet_interface.layers import (
    CONF_ADDRESSING,
    check_broadcast_link,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import HexInt, MACAddress
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


def _peer_mac(value: object) -> MACAddress:
    """A device's own address, or the broadcast address; a group or all-zero address names no one device."""
    mac = cv.mac_address(value)
    if str(mac) != BROADCAST_MAC and (mac.parts[0] & 1 or not any(mac.parts)):
        raise cv.Invalid(
            f"'{mac}' is not the address of a device; use a device's MAC address, or {BROADCAST_MAC}"
        )
    return mac


def _validate_addressing(config: ConfigType) -> ConfigType:
    if _is_broadcast(config):
        check_broadcast_link(config)
    elif CONF_ADDRESSING in config:
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
            cv.Optional(CONF_PEER_ADDRESS, default=BROADCAST_MAC): _peer_mac,
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
    # Added now, also the broadcast address, so no peer is added after setup
    cg.add(parent.add_peer(peer))
    tx_buffer = cg.static_buffer(
        config[CONF_ID], cg.uint8, config[CONF_MAX_PACKET_SIZE]
    )
    await new_packet_interface(config, parent, tx_buffer, peer)
