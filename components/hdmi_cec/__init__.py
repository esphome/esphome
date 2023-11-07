import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import CONF_ID

DEPENDENCIES = []

hdmi_cec_ns = cg.esphome_ns.namespace("hdmi_cec")
HDMICEC = hdmi_cec_ns.class_(
    "HDMICEC", cg.Component
)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HDMICEC)
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
