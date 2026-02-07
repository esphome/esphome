import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv

from .. import CONF_PACKET_INTERFACE, PacketInterface, packet_interface_ns

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["packet_interface"]

PacketInterfaceTransport = packet_interface_ns.class_(
    "PacketInterfaceTransport", PacketTransport
)

CONFIG_SCHEMA = transport_schema(PacketInterfaceTransport).extend(
    {
        cv.GenerateID(CONF_PACKET_INTERFACE): cv.use_id(PacketInterface),
    }
)


async def to_code(config):
    pi = await cg.get_variable(config[CONF_PACKET_INTERFACE])
    await new_packet_transport(config, pi)
