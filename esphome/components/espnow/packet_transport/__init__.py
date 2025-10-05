"""ESP-NOW transport platform for packet_transport component."""

import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.const import CONF_ID
from esphome.cpp_types import PollingComponent
import esphome.config_validation as cv

from .. import ESPNowComponent, espnow_ns

CODEOWNERS = ["@EasilyBoredEngineer"]
DEPENDENCIES = ["espnow"]

ESPNowTransport = espnow_ns.class_("ESPNowTransport", PacketTransport, PollingComponent)

CONF_ESPNOW_ID = "espnow_id"
CONF_BROADCAST_ADDRESS = "broadcast_address"

CONFIG_SCHEMA = transport_schema(ESPNowTransport).extend(
    {
        cv.GenerateID(CONF_ESPNOW_ID): cv.use_id(ESPNowComponent),
        cv.Optional(CONF_BROADCAST_ADDRESS, default="FF:FF:FF:FF:FF:FF"): cv.mac_address,
    }
)


async def to_code(config):
    """Set up the ESP-NOW transport component."""
    var, providers = await new_packet_transport(config)

    await cg.register_parented(var, config[CONF_ESPNOW_ID])

    # Convert MAC address to individual bytes
    mac = config[CONF_BROADCAST_ADDRESS]
    mac_bytes = mac.parts

    # Pass each byte individually
    cg.add(
        var.set_broadcast_address(
            mac_bytes[0],
            mac_bytes[1],
            mac_bytes[2],
            mac_bytes[3],
            mac_bytes[4],
            mac_bytes[5],
        )
    )
	