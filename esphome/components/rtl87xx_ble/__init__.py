"""Realtek BLE controller (RTL8720C/D).

Brings the Realtek vendor GAP stack up and owns scanning; the tracker and
other consumers drive it through the C++ API. LibreTiny only compiles/links
the SDK's BT libraries (on AmebaD via the CONFIG_BT custom option requested
here; AmebaZ2 builds them unconditionally) — the beken-72xx arrangement
(bk72xx_ble), applied to Realtek.
"""

import logging

import esphome.codegen as cg
from esphome.components import libretiny
from esphome.components.libretiny.const import FAMILY_RTL8720C, FAMILY_RTL8720D
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import EsphomeError
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@Bl00d-B0b"]
# wifi: the coexistence bring-up follows the STA; without it the stack could
# never start (setup() also fails fast at runtime for safety).
DEPENDENCIES = ["rtl87xx", "wifi"]

rtl87xx_ble_ns = cg.esphome_ns.namespace("rtl87xx_ble")
RTL87xxBLE = rtl87xx_ble_ns.class_("RTL87xxBLE", cg.Component)


def _unsupported_family_message(family: str) -> str | None:
    if family in (FAMILY_RTL8720D, FAMILY_RTL8720C):
        return None
    # AmebaZ (RTL8710B) has no BLE radio.
    return f"rtl87xx_ble requires an RTL8720C/D board; {family} has no BLE radio"


def _final_validate(config: ConfigType) -> ConfigType:
    # Warn only: a hard error here would break validate-only CI fixtures,
    # which run on an RTL8710B board. The hard error is raised at codegen.
    family = libretiny.get_libretiny_family()
    if msg := _unsupported_family_message(family):
        _LOGGER.warning("%s (this configuration cannot compile)", msg)
    elif family == FAMILY_RTL8720C:
        # LibreTiny links the AmebaZ2 BT stack in its releases and this code
        # compiles for it (CI does), but no hardware run has happened yet.
        _LOGGER.warning(
            "RTL8720C (AmebaZ2) support is compile-tested only; "
            "not yet verified on hardware"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RTL87xxBLE),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    if msg := _unsupported_family_message(libretiny.get_libretiny_family()):
        raise EsphomeError(msg)

    cg.add_define("USE_RTL87XX_BLE")

    if libretiny.get_libretiny_family() == FAMILY_RTL8720D:
        # AmebaD gates the Realtek GAP stack (btgap.a) and its HCI/coex glue
        # behind this option; AmebaZ2 compiles them unconditionally.
        cg.add_platformio_option("custom_options.platform_opts_bt#h", ["CONFIG_BT=1"])

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
