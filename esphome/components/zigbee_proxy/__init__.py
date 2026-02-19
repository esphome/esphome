import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_POWER_SAVE_MODE, CONF_WIFI
import esphome.final_validate as fv

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]

CONF_BUFFER_SIZE = "buffer_size"
CONF_INITIAL_TIMEOUT = "initial_timeout"
CONF_MIN_TIMEOUT = "min_timeout"
CONF_MAX_TIMEOUT = "max_timeout"

zigbee_proxy_ns = cg.esphome_ns.namespace("zigbee_proxy")
ZigbeeProxy = zigbee_proxy_ns.class_("ZigbeeProxy", cg.Component, uart.UARTDevice)


def final_validate(config):
    full_config = fv.full_config.get()
    if (wifi_conf := full_config.get(CONF_WIFI)) and (
        wifi_conf.get(CONF_POWER_SAVE_MODE, "").lower() != "none"
    ):
        raise cv.Invalid(
            f"{CONF_WIFI} {CONF_POWER_SAVE_MODE} must be set to 'none' when using Zigbee proxy"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ZigbeeProxy),
            cv.Optional(CONF_BUFFER_SIZE): cv.SplitDefault(
                cv.int_range(min=256, max=2048),
                esp8266=512,
                default=1024,
            ),
            cv.Optional(CONF_INITIAL_TIMEOUT, default=1600): cv.int_range(
                min=100, max=10000
            ),
            cv.Optional(CONF_MIN_TIMEOUT, default=400): cv.int_range(min=100, max=5000),
            cv.Optional(CONF_MAX_TIMEOUT, default=3200): cv.int_range(
                min=500, max=10000
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    cg.add_define("USE_ZIGBEE_PROXY")

    # Set buffer size via define for compile-time allocation
    if CONF_BUFFER_SIZE in config:
        cg.add_define("ZIGBEE_PROXY_BUFFER_SIZE", config[CONF_BUFFER_SIZE])

    # Set timeout values
    cg.add(var.set_initial_timeout(config[CONF_INITIAL_TIMEOUT]))
    cg.add(var.set_min_timeout(config[CONF_MIN_TIMEOUT]))
    cg.add(var.set_max_timeout(config[CONF_MAX_TIMEOUT]))
