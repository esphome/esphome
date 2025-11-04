import esphome.codegen as cg
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
import esphome.config_validation as cv
from esphome.cpp_types import PollingComponent

from .. import CONF_SX126X_ID, SX126x, SX126xListener, sx126x_ns

SX126xTransport = sx126x_ns.class_(
    "SX126xTransport", PacketTransport, PollingComponent, SX126xListener
)

CONFIG_SCHEMA = transport_schema(SX126xTransport).extend(
    {
        cv.GenerateID(CONF_SX126X_ID): cv.use_id(SX126x),
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    sx126x = await cg.get_variable(config[CONF_SX126X_ID])
    cg.add(var.set_parent(sx126x))
