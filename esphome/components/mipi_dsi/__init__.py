from typing import TypedDict

import esphome.codegen as cg
from esphome.core import CORE, ID

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "mipi_dsi"

mipi_dsi_ns = cg.esphome_ns.namespace("mipi_dsi")


class MipiDsiData(TypedDict):
    framebuffer_readback_ids: list[ID]
    rgb565_transform_ids: list[ID]


def get_data() -> MipiDsiData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = MipiDsiData(
            framebuffer_readback_ids=[], rgb565_transform_ids=[]
        )
    return CORE.data[DOMAIN]
