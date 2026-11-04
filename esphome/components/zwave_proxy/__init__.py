import esphome.codegen as cg
from esphome.components import uart
from esphome.components.wifi import DOMAIN as WIFI_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_POWER_SAVE_MODE, CONF_WIFI
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]
DOMAIN = "zwave_proxy"

zwave_proxy_ns = cg.esphome_ns.namespace("zwave_proxy")
ZWaveProxy = zwave_proxy_ns.class_("ZWaveProxy", cg.Component, uart.UARTDevice)


def final_validate(config: ConfigType) -> None:
    full_config = fv.full_config.get()
    if (wifi_conf := full_config.get(WIFI_DOMAIN)) and (
        wifi_conf.get(CONF_POWER_SAVE_MODE).lower() != "none"
    ):
        raise cv.Invalid(
            f"{CONF_WIFI} {CONF_POWER_SAVE_MODE} must be set to 'none' when using Z-Wave proxy"
        )


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZWaveProxy),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
    cg.add_define("USE_ZWAVE_PROXY")
