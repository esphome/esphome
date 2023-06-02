from dataclasses import dataclass
from typing import Callable, Dict

import esphome.codegen as cg


@dataclass
class LibreTinyFamily:
    family: str
    boards: Dict[str, Dict[str, str]]
    board_pins: Dict[str, Dict[str, int]]
    pin_validation: Callable[[int], int]
    usage_validation: Callable[[dict], dict]


CONF_LIBRETINY = "libretiny"
CONF_LT_CONFIG = "lt_config"
CONF_LOGLEVEL = "loglevel"
CONF_SDK_SILENT = "sdk_silent"
CONF_SDK_SILENT_ALL = "sdk_silent_all"
CONF_GPIO_RECOVER = "gpio_recover"

LT_LOGLEVELS = [
    "VERBOSE",
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
]

KEY_LIBRETINY = "libretiny"
KEY_BOARD = "board"
KEY_FAMILY = "family"
KEY_VARIANT = "variant"
KEY_FAMILY_OBJ = "family_obj"

# FAMILIES - auto-generated! Do not modify this block.
FAMILY_BK72XX = "BK72XX"
FAMILY_RTL87XX = "RTL87XX"
# FAMILIES - end

# VARIANTS - auto-generated! Do not modify this block.
VARIANT_BK7231N = "BK7231N"
VARIANT_BK7231U = "BK7231U"
VARIANT_BK7251 = "BK7251"
VARIANT_RTL8710B = "RTL8710B"
VARIANTS = [
    VARIANT_BK7231N,
    VARIANT_BK7231U,
    VARIANT_BK7251,
    VARIANT_RTL8710B,
]
VARIANT_FRIENDLY = {
    VARIANT_BK7231N: "BK7231N",
    VARIANT_BK7231U: "BK7231U",
    VARIANT_BK7251: "BK7251",
    VARIANT_RTL8710B: "RTL8710B",
}
VARIANT_FAMILY = {
    VARIANT_BK7231N: FAMILY_BK72XX,
    VARIANT_BK7231U: FAMILY_BK72XX,
    VARIANT_BK7251: FAMILY_BK72XX,
    VARIANT_RTL8710B: FAMILY_RTL87XX,
}
# VARIANTS - end

libretiny_ns = cg.esphome_ns.namespace("libretiny")
LTComponent = libretiny_ns.class_("LTComponent", cg.PollingComponent)
