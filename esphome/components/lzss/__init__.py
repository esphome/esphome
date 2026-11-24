"""LZSS compression with a preset dictionary, for short blocks of data such as packets."""

from dataclasses import dataclass

import esphome.codegen as cg
from esphome.core import CORE
from esphome.cpp_generator import MockObj

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "lzss"

# Offsets are sized by a history of at most this many bytes (WINDOW in lzss.h), so a longer dictionary would not help
MAX_DICTIONARY_SIZE = 512

lzss_ns = cg.esphome_ns.namespace("lzss")
Lzss = lzss_ns.class_("Lzss")


@dataclass
class LzssData:
    chains: MockObj | None = None


def _get_data() -> LzssData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = LzssData()
    return CORE.data[DOMAIN]


def shared_chains() -> MockObj:
    """The working space for compress(), one for every user, as compress() needs it only while it runs."""
    data = _get_data()
    if data.chains is None:
        buffer = cg.static_buffer(
            cg.unique_global_id("lzss_chains", cg.uint16),
            cg.uint16,
            Lzss.static_("CHAIN_WORDS"),
            psram=True,
        )
        # With PSRAM the buffer is an allocation, so it is made once and kept
        data.chains = cg.variable(
            cg.unique_global_id("lzss_chains_ptr", cg.uint16.operator("ptr")),
            buffer,
        )
    return data.chains
