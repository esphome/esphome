import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code, OTAComponent
from esphome.const import (
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    CONF_VERSION,
)
from esphome.core import coroutine_with_priority


CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["md5", "socket"]
DEPENDENCIES = ["network"]

esphome = cg.esphome_ns.namespace("esphome")
ESPHomeOTAComponent = esphome.class_("ESPHomeOTAComponent", OTAComponent)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPHomeOTAComponent),
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2, int=True),
            cv.SplitDefault(
                CONF_PORT,
                esp8266=8266,
                esp32=3232,
                rp2040=2040,
                bk72xx=8892,
                rtl87xx=8892,
            ): cv.port,
            cv.Optional(CONF_PASSWORD): cv.string,
            cv.Optional(CONF_NUM_ATTEMPTS): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
            cv.Optional(CONF_REBOOT_TIMEOUT): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
            cv.Optional(CONF_SAFE_MODE): cv.invalid(
                f"'{CONF_SAFE_MODE}' (and its related configuration variables) has moved from 'ota' to its own component. See https://esphome.io/components/safe_mode"
            ),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


@coroutine_with_priority(52.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    if CONF_PASSWORD in config:
        cg.add(var.set_auth_password(config[CONF_PASSWORD]))
        cg.add_define("USE_OTA_PASSWORD")
    cg.add_define("USE_OTA_VERSION", config[CONF_VERSION])

    await cg.register_component(var, config)
