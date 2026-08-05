import esphome.codegen as cg
from esphome.components import serial_proxy
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_POWER_SAVE_MODE, CONF_WIFI
import esphome.final_validate as fv

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "serial_proxy"]

CONF_INITIAL_TIMEOUT = "initial_timeout"
CONF_MIN_TIMEOUT = "min_timeout"
CONF_MAX_TIMEOUT = "max_timeout"
CONF_SERIAL_PROXY_ID = "serial_proxy_id"

# Default ACK timeout values for the boot-time metadata harvest
_DEFAULT_INITIAL_TIMEOUT = 1600
_DEFAULT_MIN_TIMEOUT = 400
_DEFAULT_MAX_TIMEOUT = 3200

zigbee_proxy_ns = cg.esphome_ns.namespace("zigbee_proxy")
ZigbeeProxy = zigbee_proxy_ns.class_(
    "ZigbeeProxy", cg.Component, serial_proxy.SerialProxyTap
)


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
            cv.Required(CONF_SERIAL_PROXY_ID): cv.use_id(serial_proxy.SerialProxy),
            cv.Optional(CONF_BUFFER_SIZE): cv.SplitDefault(
                cv.int_range(min=256, max=2048),
                esp8266=512,
                default=1024,
            ),
            cv.Optional(
                CONF_INITIAL_TIMEOUT, default=_DEFAULT_INITIAL_TIMEOUT
            ): cv.int_range(min=10, max=10000),
            cv.Optional(CONF_MIN_TIMEOUT, default=_DEFAULT_MIN_TIMEOUT): cv.int_range(
                min=10, max=5000
            ),
            cv.Optional(CONF_MAX_TIMEOUT, default=_DEFAULT_MAX_TIMEOUT): cv.int_range(
                min=50, max=10000
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)

FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sp = await cg.get_variable(config[CONF_SERIAL_PROXY_ID])
    cg.add(var.set_serial_proxy(sp))

    cg.add_define("USE_ZIGBEE_PROXY")
    # Compiles the tap interface into serial_proxy; without it the port is a plain byte pipe
    cg.add_define("USE_SERIAL_PROXY_TAP")

    # Set buffer size via define for compile-time allocation
    if CONF_BUFFER_SIZE in config:
        cg.add_define("ZIGBEE_PROXY_BUFFER_SIZE", config[CONF_BUFFER_SIZE])

    cg.add(var.set_initial_timeout(config[CONF_INITIAL_TIMEOUT]))
    cg.add(var.set_min_timeout(config[CONF_MIN_TIMEOUT]))
    cg.add(var.set_max_timeout(config[CONF_MAX_TIMEOUT]))
