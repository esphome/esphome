import esphome.codegen as cg
from esphome.components import serial_proxy
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@puddly"]
DEPENDENCIES = ["serial_proxy"]
DOMAIN = "zwave_proxy_tap"

CONF_SERIAL_PROXY_ID = "serial_proxy_id"

zwave_proxy_tap_ns = cg.esphome_ns.namespace("zwave_proxy_tap")
ZWaveProxyTap = zwave_proxy_tap_ns.class_(
    "ZWaveProxyTap", cg.Component, serial_proxy.SerialProxyTap
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZWaveProxyTap),
        cv.Required(CONF_SERIAL_PROXY_ID): cv.use_id(serial_proxy.SerialProxy),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    sp = await cg.get_variable(config[CONF_SERIAL_PROXY_ID])
    var = cg.new_Pvariable(config[CONF_ID], sp)
    await cg.register_component(var, config)

    cg.add_define("USE_ZWAVE_PROXY_TAP")
    cg.add_define("USE_SERIAL_PROXY_TAP")
