import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID

DEPENDENCIES = ["api"]

sndbuf_pin_ns = cg.esphome_ns.namespace("sndbuf_pin")
SndbufPinComponent = sndbuf_pin_ns.class_("SndbufPinComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SndbufPinComponent),
        cv.Required(CONF_BUFFER_SIZE): cv.int_range(min=1),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_BUFFER_SIZE])
    await cg.register_component(var, config)
