import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.cpp_types import PollingComponent

from .. import CC1101Component, CC1101Listener, ns as cc1101_ns

CONF_CC1101_ID = "cc1101_id"

CC1101Transport = cc1101_ns.class_(
    "CC1101Transport", PacketTransport, PollingComponent, CC1101Listener
)

CONFIG_SCHEMA = transport_schema(CC1101Transport).extend(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    await cg.register_parented(var, config[CONF_CC1101_ID])
