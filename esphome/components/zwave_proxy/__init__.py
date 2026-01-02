import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_POWER_SAVE_MODE, CONF_WIFI
import esphome.final_validate as fv

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]

zwave_proxy_ns = cg.esphome_ns.namespace("zwave_proxy")
ZWaveProxy = zwave_proxy_ns.class_("ZWaveProxy", cg.Component, uart.UARTDevice)


def final_validate(config):
    full_config = fv.full_config.get()
    if (wifi_conf := full_config.get(CONF_WIFI)) and (
        wifi_conf.get(CONF_POWER_SAVE_MODE).lower() != "none"
    ):
        raise cv.Invalid(
            f"{CONF_WIFI} {CONF_POWER_SAVE_MODE} must be set to 'none' when using Z-Wave proxy"
        )

    return config


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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add_define("USE_ZWAVE_PROXY")

    # Request UART to wake the main loop when data arrives for low-latency processing
    uart.request_wake_loop_on_rx()
