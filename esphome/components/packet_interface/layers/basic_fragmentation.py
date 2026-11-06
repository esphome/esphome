"""Fragmentation layer that splits packets larger than the layers below allow, and joins them again."""

import math

import esphome.codegen as cg
from esphome.components.const import CONF_MAX_PACKET_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import FRAGMENTATION, PacketInterface, packet_interface_ns

# The packet number, and the fragment's index with the flag on the last fragment
HEADER_SIZE = 2
MAX_FRAGMENTS = 128

BasicFragmentation = packet_interface_ns.class_("BasicFragmentation", PacketInterface)

BASIC_FRAGMENTATION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BasicFragmentation),
        # The packet being joined is held in a buffer of this size
        cv.Optional(CONF_MAX_PACKET_SIZE, default=1024): cv.int_range(16, 16384),
    }
).extend(cv.COMPONENT_SCHEMA)


def _check_lower(config: ConfigType, size: int, capacity: int | None) -> None:
    """The largest packet must fit in the fragments that the layer below can take at once."""
    fragment_size = size - HEADER_SIZE
    if fragment_size <= 0:
        raise cv.Invalid("The layers below leave no room for fragments")
    fragments = max(1, math.ceil(config[CONF_MAX_PACKET_SIZE] / fragment_size))
    limit = MAX_FRAGMENTS if capacity is None else min(MAX_FRAGMENTS, capacity)
    if fragments > limit:
        raise cv.Invalid(
            f"A packet of {config[CONF_MAX_PACKET_SIZE]} bytes needs {fragments} fragments of {fragment_size} bytes, "
            f"but at most {limit} can be sent at once",
            path=[CONF_MAX_PACKET_SIZE],
        )


# Fragments from different devices are told apart by the addressing layer's source address, so of two devices sending
# fragmented packets at once, one still gets through; without addressing, the receiver can drop both
@FRAGMENTATION.register(
    "basic",
    schema=BASIC_FRAGMENTATION_SCHEMA,
    packet_size=lambda config, size: config[CONF_MAX_PACKET_SIZE],
    check_lower=_check_lower,
)
async def build_basic_fragmentation(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_BASIC_FRAGMENTATION")
    var = cg.new_Pvariable(config[CONF_ID], lower, config[CONF_MAX_PACKET_SIZE])
    await cg.register_component(var, config)
    return var
