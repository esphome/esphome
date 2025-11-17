import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.packet_transport import (
    PacketTransport,
    new_packet_transport,
    transport_schema,
)
from esphome.const import CONF_ID
from esphome.cpp_types import PollingComponent

from .. import CONF_CANBUS_ID, CONF_CAN_ID, CONF_USE_EXTENDED_ID, canbus_ns

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["canbus"]

Canbus = canbus_ns.class_("Canbus", cg.Component)
CanbusTransport = canbus_ns.class_("CanbusTransport", PacketTransport, PollingComponent)

CONFIG_SCHEMA = transport_schema(CanbusTransport).extend(
    {
        cv.GenerateID(CONF_CANBUS_ID): cv.use_id(Canbus),
        cv.Optional(CONF_CAN_ID, default=0x600): cv.int_range(min=0, max=0x1FFFFFFF),
        cv.Optional(CONF_USE_EXTENDED_ID, default=False): cv.boolean,
    }
)


async def to_code(config):
    var, _ = await new_packet_transport(config)
    canbus_var = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_parent(canbus_var))
    cg.add(var.set_can_id(config[CONF_CAN_ID]))
    cg.add(var.set_use_extended_id(config[CONF_USE_EXTENDED_ID]))
