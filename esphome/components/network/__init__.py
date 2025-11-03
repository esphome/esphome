import ipaddress

import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_IPV6, CONF_MIN_IPV6_ADDR_COUNT
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["mdns"]

network_ns = cg.esphome_ns.namespace("network")
IPAddress = network_ns.class_("IPAddress")


def ip_address_literal(ip: str | int | None) -> cg.MockObj:
    """Generate an IPAddress with compile-time initialization instead of runtime parsing.

    This function parses the IP address in Python during code generation and generates
    a call to the 4-octet constructor (IPAddress(192, 168, 1, 1)) instead of the
    string constructor (IPAddress("192.168.1.1")). This eliminates runtime string
    parsing overhead and reduces flash usage on embedded systems.

    Args:
        ip: IP address as string (e.g., "192.168.1.1"), ipaddress.IPv4Address, or None

    Returns:
        IPAddress expression that uses 4-octet constructor for efficiency
    """
    if ip is None:
        return IPAddress(0, 0, 0, 0)

    try:
        # Parse using Python's ipaddress module
        ip_obj = ipaddress.ip_address(ip)
    except (ValueError, TypeError):
        pass
    else:
        # Only support IPv4 for now
        if isinstance(ip_obj, ipaddress.IPv4Address):
            # Extract octets from the packed bytes representation
            octets = ip_obj.packed
            # Generate call to 4-octet constructor: IPAddress(192, 168, 1, 1)
            return IPAddress(octets[0], octets[1], octets[2], octets[3])

    # Fallback to string constructor if parsing fails
    return IPAddress(str(ip))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.SplitDefault(
            CONF_ENABLE_IPV6,
            esp8266=False,
            esp32=False,
            rp2040=False,
            bk72xx=False,
            host=False,
        ): cv.All(
            cv.boolean,
            cv.Any(
                cv.require_framework_version(
                    esp_idf=cv.Version(0, 0, 0),
                    esp32_arduino=cv.Version(0, 0, 0),
                    esp8266_arduino=cv.Version(0, 0, 0),
                    rp2040_arduino=cv.Version(0, 0, 0),
                    bk72xx_arduino=cv.Version(1, 7, 0),
                    host=cv.Version(0, 0, 0),
                ),
                cv.boolean_false,
            ),
        ),
        cv.Optional(CONF_MIN_IPV6_ADDR_COUNT, default=0): cv.positive_int,
    }
)


@coroutine_with_priority(CoroPriority.NETWORK)
async def to_code(config):
    cg.add_define("USE_NETWORK")
    if CORE.using_arduino and CORE.is_esp32:
        cg.add_library("Networking", None)
    if (enable_ipv6 := config.get(CONF_ENABLE_IPV6, None)) is not None:
        cg.add_define("USE_NETWORK_IPV6", enable_ipv6)
        if enable_ipv6:
            cg.add_define(
                "USE_NETWORK_MIN_IPV6_ADDR_COUNT", config[CONF_MIN_IPV6_ADDR_COUNT]
            )
        if CORE.is_esp32:
            if CORE.using_esp_idf:
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", enable_ipv6)
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", enable_ipv6)
            else:
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6", True)
                add_idf_sdkconfig_option("CONFIG_LWIP_IPV6_AUTOCONFIG", True)
        elif enable_ipv6:
            cg.add_build_flag("-DCONFIG_LWIP_IPV6")
            cg.add_build_flag("-DCONFIG_LWIP_IPV6_AUTOCONFIG")
            if CORE.is_rp2040:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_ENABLE_IPV6")
            if CORE.is_esp8266:
                cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY")
            if CORE.is_bk72xx:
                cg.add_build_flag("-DCONFIG_IPV6")
