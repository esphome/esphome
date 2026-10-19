"""Fragmentation layer that splits packets larger than the layers below allow, and joins them again."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import FRAGMENTATION, PacketInterface, packet_interface_ns

CONF_MAX_PACKET_SIZE = "max_packet_size"

BasicFragmentation = packet_interface_ns.class_("BasicFragmentation", PacketInterface)

BASIC_FRAGMENTATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BasicFragmentation),
        # The packet being joined is held in a buffer of this size
        cv.Optional(CONF_MAX_PACKET_SIZE, default=1024): cv.int_range(16, 16384),
    }
).extend(cv.COMPONENT_SCHEMA)


# Fragments from different devices are told apart by the addressing layer's source address, so of two devices sending
# fragmented packets at once, one still gets through; without addressing, the receiver can drop both
@FRAGMENTATION.register("basic", schema=BASIC_FRAGMENTATION_SCHEMA)
async def build_basic_fragmentation(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_BASIC_FRAGMENTATION")
    var = cg.new_Pvariable(config[CONF_ID], lower, config[CONF_MAX_PACKET_SIZE])
    await cg.register_component(var, config)
    return var
