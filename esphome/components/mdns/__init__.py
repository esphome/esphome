import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]

CONF_DISABLED = "disabled"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DISABLED, default=False): cv.boolean,
    }
)


async def to_code(config):
    if config[CONF_DISABLED]:
        return

    cg.add_define("USE_MDNS")
    if CORE.using_arduino:
        if CORE.is_esp32:
            cg.add_library("ESPmDNS", None)
        elif CORE.is_esp8266:
            cg.add_library("ESP8266mDNS", None)
