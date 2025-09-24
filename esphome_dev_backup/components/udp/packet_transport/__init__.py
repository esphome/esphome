import esphome.codegen as cg
from esphome.components.api import CONF_ENCRYPTION
from esphome.components.packet_transport import (
    CONF_PING_PONG_ENABLE,
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.const import CONF_BINARY_SENSORS, CONF_SENSORS
from esphome.cpp_types import PollingComponent

from .. import UDP_SCHEMA, register_udp_client, udp_ns

UDPTransport = udp_ns.class_("UDPTransport", PacketTransport, PollingComponent)

CONFIG_SCHEMA = transport_schema(UDPTransport).extend(UDP_SCHEMA)


async def to_code(config):
    var, providers = await new_packet_transport(config)
    udp_var = await register_udp_client(var, config)
    if CONF_ENCRYPTION in config or providers:
        cg.add(udp_var.set_should_listen())
    if (
        config[CONF_PING_PONG_ENABLE]
        or config.get(CONF_SENSORS, ())
        or config.get(CONF_BINARY_SENSORS, ())
    ):
        cg.add(udp_var.set_should_broadcast())
