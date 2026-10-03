import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    new_packet_interface,
    packet_interface_schema,
)

from .. import CONF_UDP_ID, UDP_SCHEMA, udp_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["udp"]

UdpPacketInterface = udp_ns.class_(
    "UdpPacketInterface",
    packet_interface.PacketInterface,
)

CONFIG_SCHEMA = packet_interface_schema(UdpPacketInterface).extend(UDP_SCHEMA)


async def to_code(config):
    udp_id = await cg.get_variable(config[CONF_UDP_ID])
    await new_packet_interface(config, udp_id)
