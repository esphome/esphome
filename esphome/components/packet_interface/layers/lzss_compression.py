"""Compression layer using LZSS with a preset dictionary, compressing each packet on its own."""

import zlib

import esphome.codegen as cg
from esphome.components.const import CONF_MAX_PACKET_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import COMPRESSION, PacketInterface, packet_interface_ns, remaining_packet_size

CONF_DICTIONARY = "dictionary"

# Whether the packet is compressed, and the dictionary check
HEADER_SIZE = 1
MAX_DICTIONARY_SIZE = 1024

# Pieces common in JSON, for when no dictionary is given. One made from a typical packet of the application does much
# better, as most of a short packet is then copied from it.
DEFAULT_DICTIONARY = (
    '{"name":"","value":,"state":"ON","OFF","unit":"","id":"","type":"temperature","humidity",'
    '"time":"","status":"ok","error":null,"enabled":true,"false},{"":[{""}]}'
)

LzssCompression = packet_interface_ns.class_("LzssCompression", PacketInterface)

LZSS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LzssCompression),
        # Text that both ends hold, which packets copy from; both ends must have the same
        cv.Optional(CONF_DICTIONARY, default=DEFAULT_DICTIONARY): cv.All(
            cv.string_strict, cv.ByteLength(max=MAX_DICTIONARY_SIZE)
        ),
        # A packet larger than the layer below takes is sent only if it compresses enough
        cv.Optional(CONF_MAX_PACKET_SIZE): cv.int_range(16, 16384),
    }
).extend(cv.COMPONENT_SCHEMA)


def _packet_size(config: ConfigType, size: int) -> int:
    return config.get(CONF_MAX_PACKET_SIZE, remaining_packet_size(size, HEADER_SIZE))


def _check_lower(config: ConfigType, size: int, capacity: int | None) -> None:
    if size <= HEADER_SIZE:
        raise cv.Invalid("The layers below leave no room for compressed packets")


def dictionary_check(dictionary: bytes) -> int:
    """Seven bits that differ between most dictionaries, sent in every header."""
    return zlib.crc32(dictionary) & 0x7F


@COMPRESSION.register(
    "lzss",
    schema=LZSS_SCHEMA,
    packet_size=_packet_size,
    check_lower=_check_lower,
)
async def build_lzss(config: ConfigType, lower: MockObj) -> MockObj:
    cg.add_define("USE_PACKET_INTERFACE_LZSS")
    dictionary = config[CONF_DICTIONARY].encode()
    table = (
        cg.shared_progmem_array(
            "packet_interface_dictionary", cg.uint8, list(dictionary)
        )
        if dictionary
        else cg.nullptr
    )
    var = cg.new_Pvariable(
        config[CONF_ID],
        lower,
        config.get(CONF_MAX_PACKET_SIZE, 0),
        table,
        len(dictionary),
        dictionary_check(dictionary),
    )
    await cg.register_component(var, config)
    return var
