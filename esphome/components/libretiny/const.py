from dataclasses import dataclass
from typing import Callable

import esphome.codegen as cg


@dataclass
class LibreTinyComponent:
    name: str
    boards: dict[str, dict[str, str]]
    board_pins: dict[str, dict[str, int]]
    pin_validation: Callable[[int], int]
    usage_validation: Callable[[dict], dict]


CONF_LIBRETINY = "libretiny"
CONF_LOGLEVEL = "loglevel"
CONF_SDK_SILENT = "sdk_silent"
CONF_GPIO_RECOVER = "gpio_recover"
CONF_UART_PORT = "uart_port"

LT_LOGLEVELS = [
    "VERBOSE",
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
    "NONE",
]

LT_DEBUG_MODULES = [
    "WIFI",
    "CLIENT",
    "SERVER",
    "SSL",
    "OTA",
    "FDB",
    "MDNS",
    "LWIP",
    "LWIP_ASSERT",
]

KEY_LIBRETINY = "libretiny"
KEY_BOARD = "board"
KEY_COMPONENT = "component"
KEY_COMPONENT_DATA = "component_data"

# COMPONENTS - auto-generated! Do not modify this block.
COMPONENT_BK72XX = "bk72xx"
COMPONENT_RTL87XX = "rtl87xx"
# COMPONENTS - end

# VARIANTS - auto-generated! Do not modify this block.
VARIANT_BK7231N = "BK7231N"
VARIANT_BK7231Q = "BK7231Q"
VARIANT_BK7231T = "BK7231T"
VARIANT_BK7252 = "BK7252"
VARIANT_RTL8710BN = "RTL8710BN"
VARIANT_RTL8710BX = "RTL8710BX"
VARIANT_RTL8720CF = "RTL8720CF"
VARIANTS = [
    VARIANT_BK7231N,
    VARIANT_BK7231Q,
    VARIANT_BK7231T,
    VARIANT_BK7252,
    VARIANT_RTL8710BN,
    VARIANT_RTL8710BX,
    VARIANT_RTL8720CF,
]
VARIANT_FRIENDLY = {
    VARIANT_BK7231N: "BK7231N",
    VARIANT_BK7231Q: "BK7231Q",
    VARIANT_BK7231T: "BK7231T",
    VARIANT_BK7252: "BK7252",
    VARIANT_RTL8710BN: "RTL8710BN",
    VARIANT_RTL8710BX: "RTL8710BX",
    VARIANT_RTL8720CF: "RTL8720CF",
}
VARIANT_COMPONENT = {
    VARIANT_BK7231N: COMPONENT_BK72XX,
    VARIANT_BK7231Q: COMPONENT_BK72XX,
    VARIANT_BK7231T: COMPONENT_BK72XX,
    VARIANT_BK7252: COMPONENT_BK72XX,
    VARIANT_RTL8710BN: COMPONENT_RTL87XX,
    VARIANT_RTL8710BX: COMPONENT_RTL87XX,
    VARIANT_RTL8720CF: COMPONENT_RTL87XX,
}
# VARIANTS - end

libretiny_ns = cg.esphome_ns.namespace("libretiny")
LTComponent = libretiny_ns.class_("LTComponent", cg.PollingComponent)
