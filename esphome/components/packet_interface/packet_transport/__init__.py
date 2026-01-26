import esphome.codegen as cg
from esphome.components import packet_interface
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)

from .. import packet_interface_ns

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["packet_interface"]

PacketInterfaceTransport = packet_interface_ns.class_(
    "PacketInterfaceTransport", PacketTransport
)

CONFIG_SCHEMA = transport_schema(PacketInterfaceTransport).extend(
    {
        cg.Required(packet_interface.CONF_PACKET_INTERFACE): cg.use_id(
            packet_interface.PacketInterface
        ),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    pi = await cg.get_variable(config[packet_interface.CONF_PACKET_INTERFACE])
    cg.add(var.set_packet_interface(pi))
    cg.add(pi.add_packet_interface_listener(var.method("on_packet_")))
