import esphome.codegen as cg
from esphome.components import serial_proxy
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_POWER_SAVE_MODE, CONF_WIFI
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["serial_proxy"]

CONF_SERIAL_PROXY_ID = "serial_proxy_id"

zigbee_proxy_ns = cg.esphome_ns.namespace("zigbee_proxy")
ZigbeeProxy = zigbee_proxy_ns.class_(
    "ZigbeeProxy", cg.Component, serial_proxy.SerialProxyTap
)


def _final_validate(config: ConfigType) -> ConfigType:
    full_config = fv.full_config.get()
    if (wifi_conf := full_config.get(CONF_WIFI)) and (
        wifi_conf.get(CONF_POWER_SAVE_MODE, "").lower() != "none"
    ):
        raise cv.Invalid(
            f"{CONF_WIFI} {CONF_POWER_SAVE_MODE} must be set to 'none' when using Zigbee proxy"
        )
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ZigbeeProxy),
        cv.Required(CONF_SERIAL_PROXY_ID): cv.use_id(serial_proxy.SerialProxy),
    }
).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    sp = await cg.get_variable(config[CONF_SERIAL_PROXY_ID])
    var = cg.new_Pvariable(config[CONF_ID], sp)
    await cg.register_component(var, config)

    cg.add_define("USE_ZIGBEE_PROXY")
    # Compiles the tap interface into serial_proxy; without it the port is a plain byte pipe
    cg.add_define("USE_SERIAL_PROXY_TAP")
