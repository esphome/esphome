from typing import Any

import esphome.codegen as cg
from esphome.components.const import CONF_MAX_PACKET_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "packet_interface"
IS_PLATFORM_COMPONENT = True

CONF_PACKET_INTERFACE = "packet_interface"

packet_interface_ns = cg.esphome_ns.namespace("packet_interface")
PacketInterface = packet_interface_ns.class_("PacketInterface", cg.Component)

__all__ = [
    "CONF_MAX_PACKET_SIZE",
    "CONF_PACKET_INTERFACE",
    "PacketInterface",
    "new_packet_interface",
    "packet_interface_ns",
    "packet_interface_schema",
]


def packet_interface_schema(
    class_: MockObjClass,
    default_max_packet_size: int,
    max_packet_size_limit: int = 4096,
) -> cv.Schema:
    """Schema for a link platform."""
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.Optional(
                CONF_MAX_PACKET_SIZE, default=default_max_packet_size
            ): cv.int_range(16, max_packet_size_limit),
        }
    ).extend(cv.COMPONENT_SCHEMA)


async def new_packet_interface(config: ConfigType, *args: Any) -> MockObj:
    """Create the link layer.

    The link's constructor takes its maximum packet size, usually as a std::span over a buffer that code generation
    sized from the configuration, so the two cannot differ.

    Returns the link layer; applications reach the top layer with PacketInterface::top().
    """
    var = cg.new_Pvariable(config[CONF_ID], *args)
    cg.add(var.set_name(config[CONF_ID].id))
    await cg.register_component(var, config)
    return var
