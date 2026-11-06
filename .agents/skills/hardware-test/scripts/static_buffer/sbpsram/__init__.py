"""Test-only component: a cg.static_buffer(psram=True) buffer.

With PSRAM enabled, codegen passes psram_buffer() instead of the static array.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SIZE
from esphome.types import ConfigType

DOMAIN = "sbpsram"
MULTI_CONF = True
ns = cg.esphome_ns.namespace("sbpsram")
SBPsram = ns.class_("SBPsram", cg.Component)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SBPsram),
        cv.Optional(CONF_SIZE, default=4096): cv.int_range(min=1, max=200000),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    size = config[CONF_SIZE]
    p = cg.static_buffer(config[CONF_ID], cg.uint8, size, psram=True)
    s = cg.static_buffer(config[CONF_ID], cg.uint32, 8)
    var = cg.new_Pvariable(config[CONF_ID], p, size, s)
    await cg.register_component(var, config)
