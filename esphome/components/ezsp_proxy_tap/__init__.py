import esphome.codegen as cg
from esphome.components import serial_proxy
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81", "@puddly"]
DEPENDENCIES = ["serial_proxy"]

CONF_SERIAL_PROXY_ID = "serial_proxy_id"

ezsp_proxy_tap_ns = cg.esphome_ns.namespace("ezsp_proxy_tap")
EzspProxyTap = ezsp_proxy_tap_ns.class_(
    "EzspProxyTap", cg.Component, serial_proxy.SerialProxyTap
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(EzspProxyTap),
        cv.Required(CONF_SERIAL_PROXY_ID): cv.use_id(serial_proxy.SerialProxy),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    sp = await cg.get_variable(config[CONF_SERIAL_PROXY_ID])
    var = cg.new_Pvariable(config[CONF_ID], sp)
    await cg.register_component(var, config)

    cg.add_define("USE_EZSP_PROXY_TAP")
    # Compiles the tap interface into serial_proxy; without it the port is a plain byte pipe
    cg.add_define("USE_SERIAL_PROXY_TAP")
