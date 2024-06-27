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
)

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

network_ns = cg.esphome_ns.namespace("network")
IPAddress = network_ns.class_("IPAddress")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(
            CONF_ENABLE_IPV6,
            esp8266=False,
            esp32=False,
            rp2040=False,
        ): cv.All(
            cv.boolean, cv.only_on([PLATFORM_ESP32, PLATFORM_ESP8266, PLATFORM_RP2040])
        ),
        cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
    }
)


async def to_code(config):
    if (enable_ipv6 := config.get(CONF_ENABLE_IPV6, None)) is not None:
        cg.add_define("USE_NETWORK_IPV6", enable_ipv6)
        if enable_ipv6:
            cg.add_define(
                "USE_NETWORK_MIN_IPV6_ADDR_COUNT", config[CONF_MIN_IPV6_ADDR_COUNT]
            )
        if CORE.using_esp_idf:
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", enable_ipv6)
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", enable_ipv6)
        else:
            if enable_ipv6:
                cg.add_build_flag("-DCONFIG_LWIP_IPV6")
                cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG")
                if CORE.is_rp2040:
                    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
                if CORE.is_esp8266:
                    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
