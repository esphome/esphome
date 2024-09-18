import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_IPV6, CONF_MIN_IPV6_ADDR_COUNT
from esphome.core import CORE

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
            bk72xx=False,
        ): cv.All(
            cv.boolean,
            cv.Any(
                cv.require_framework_version(
                    esp_idf=cv.Version(0, 0, 0),
                    esp32_arduino=cv.Version(0, 0, 0),
                    esp8266_arduino=cv.Version(0, 0, 0),
                    rp2040_arduino=cv.Version(0, 0, 0),
                    bk72xx_libretiny=cv.Version(1, 7, 0),
                ),
                cv.boolean_false,
            ),
        ),
        cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
    }
)


async def to_code(config):
    cg.add_define("USE_NETWORK")
    if (enable_ipv6 := config.get(CONF_ENABLE_IPV6, None)) is not None:
        cg.add_define("USE_NETWORK_IPV6", enable_ipv6)
        if enable_ipv6:
            cg.add_define(
                "USE_NETWORK_MIN_IPV6_ADDR_COUNT", config[CONF_MIN_IPV6_ADDR_COUNT]
            )
        if CORE.using_esp_idf:
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", enable_ipv6)
            add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", enable_ipv6)
        elif enable_ipv6:
            cg.add_build_flag("-DCONFIG_LWIP_IPV6")
            cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG")
            if CORE.is_rp2040:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
            if CORE.is_esp8266:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
            if CORE.is_bk72xx:
                cg.add_build_flag("-DCONFIG_IPV6")
