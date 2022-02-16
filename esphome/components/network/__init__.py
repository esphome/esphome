import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option

from esphome.const import (
    CONF_ENABLE_IPV6,
)

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

network_ns = cg.esphome_ns.namespace("network")
IPAddress = network_ns.class_("IPAddress")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(CONF_ENABLE_IPV6, esp32_idf=False): cv.All(
            cv.only_with_esp_idf, cv.boolean
        ),
    }
)


async def to_code(config):
    if CONF_ENABLE_IPV6 in config and config[CONF_ENABLE_IPV6]:
        add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", True)
        add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", True)
