from esphome.core import CORE
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option

from esphome.const import (
    CONF_ENABLE_IPV6,
    CONF_MIN_IPV6_ADDR_COUNT,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_RP2040,
    CONF_HOSTS,
    CONF_ID,
    CONF_IP_ADDRESS,
    CONF_NAME,
)

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

network_ns = cg.esphome_ns.namespace("network")
IPAddress = network_ns.class_("IPAddress")
Resolver = network_ns.class_("Resolver")
CONF_NETWORK_ID = "network_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(CONF_ENABLE_IPV6): cv.All(
            cv.boolean, cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_RP2040])
        ),
        cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
        cv.GenerateID(CONF_NETWORK_ID): cv.declare_id(Resolver),
        cv.Optional(CONF_HOSTS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_NAME): cv.string,
                    cv.Required(CONF_IP_ADDRESS): cv.string,
                }
            )
        ),
    }
)


async def to_code(config):
    if CONF_ENABLE_IPV6 in config:
        cg.add_define("USE_NETWORK_IPV6", config[CONF_ENABLE_IPV6])
        cg.add_define(
            "USE_NETWORK_MIN_IPV6_ADDR_COUNT", config[CONF_MIN_IPV6_ADDR_COUNT]
    if config[CONF_HOSTS]:
        hosts = [
            (host[CONF_NAME], IPAddress(host[CONF_IP_ADDRESS]))
            for host in config[CONF_HOSTS]
        ]
    else:
        hosts = []

    map_ = cg.std_ns.class_("map").template(cg.std_string, IPAddress)
    cg.new_Pvariable(config[CONF_NETWORK_ID], map_(hosts))
    cg.add_define("ENABLE_IPV6", config[CONF_ENABLE_IPV6])
    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", config[CONF_ENABLE_IPV6])
        add_idf_sdkconfig_option(
            "CONFIG_LWIP_IPV6_AUTOCONFIG", config[CONF_ENABLE_IPV6]
        )
        if CORE.using_esp_idf:
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", config[CONF_ENABLE_IPV6])
            add_idf_sdkconfig_option(
                "CONFIG_LWIP_IPV6_AUTOCONFIG", config[CONF_ENABLE_IPV6]
            )
        else:
            if config[CONF_ENABLE_IPV6]:
                cg.add_build_flag("-DCONFIG_LWIP_IPV6")
                cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG")
                if CORE.is_rp2040:
                    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
                if CORE.is_esp8266:
                    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
