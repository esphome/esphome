"""ESP-NOW transport platform for packet_transport component."""

import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.core import HexInt
from esphome.cpp_types import PollingComponent

from .. import ESPNowComponent, espnow_ns

CODEOWNERS = ["@EasilyBoredEngineer"]
DEPENDENCIES = ["espnow"]

ESPNowTransport = espnow_ns.class_("ESPNowTransport", PacketTransport, PollingComponent)

CONF_ESPNOW_ID = "espnow_id"
CONF_PEER_ADDRESS = "peer_address"

CONFIG_SCHEMA = transport_schema(ESPNowTransport).extend(
    {
        cv.GenerateID(CONF_ESPNOW_ID): cv.use_id(ESPNowComponent),
        cv.Optional(CONF_PEER_ADDRESS, default="FF:FF:FF:FF:FF:FF"): cv.mac_address,
    }
)


async def to_code(config):
    """Set up the ESP-NOW transport component."""
    var, _ = await new_packet_transport(config)

    await cg.register_parented(var, config[CONF_ESPNOW_ID])

    # Set peer address - convert MAC to parts array like ESP-NOW does
    mac = config[CONF_PEER_ADDRESS]
    cg.add(var.set_peer_address([HexInt(x) for x in mac.parts]))
