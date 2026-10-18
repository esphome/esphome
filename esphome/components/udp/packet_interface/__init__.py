import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_interface import (
    new_packet_interface,
    packet_interface_schema,
)
from esphome.types import ConfigType

from .. import CONF_UDP_ID, UDP_SCHEMA, udp_ns

CODEOWNERS = ["@clydebarrow"]

DEPENDENCIES = ["udp"]

# Size of the receive buffer of the udp component
MAX_PACKET_SIZE = 508

UdpPacketInterface = udp_ns.class_(
    "UdpPacketInterface",
    packet_interface.PacketInterface,
)

CONFIG_SCHEMA = packet_interface_schema(
    UdpPacketInterface, MAX_PACKET_SIZE, MAX_PACKET_SIZE
).extend(UDP_SCHEMA)


async def to_code(config: ConfigType) -> None:
    udp_var = await cg.get_variable(config[CONF_UDP_ID])
    cg.add(udp_var.set_should_listen())
    # The udp component sends to its addresses through its broadcast socket
    cg.add(udp_var.set_should_broadcast())
    await new_packet_interface(config, udp_var)
