"""Test-only component: three cg.static_buffer() buffers, checked for zeroing, alignment and overlap at boot."""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DOMAIN = "sbtest"
MULTI_CONF = True
sbtest_ns = cg.esphome_ns.namespace("sbtest")
SBTest = sbtest_ns.class_("SBTest", cg.Component)
CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(SBTest)}).extend(
    cv.COMPONENT_SCHEMA
)
# A size given as a C++ expression instead of a Python int
EXPR = "sizeof(uint64_t) * 3"


async def to_code(config: ConfigType) -> None:
    a = cg.static_buffer(config[CONF_ID], cg.uint8, 37)
    b = cg.static_buffer(config[CONF_ID], cg.uint32, 5)
    c = cg.static_buffer(config[CONF_ID], cg.const_char_ptr, EXPR)
    var = cg.new_Pvariable(config[CONF_ID], a, 37, b, 5, c, cg.RawExpression(EXPR))
    await cg.register_component(var, config)
