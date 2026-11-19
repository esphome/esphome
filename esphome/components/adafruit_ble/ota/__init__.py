import logging

import esphome.codegen as cg
from esphome.components.const import CONF_ON_NUMERIC_COMPARISON_REQUEST
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT
from esphome.components.ota import (
    BASE_OTA_SCHEMA,
    CONF_ON_ABORT,
    CONF_ON_END,
    CONF_ON_PROGRESS,
    OTAComponent,
    ota_to_code,
)
from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_BOOTLOADER
from esphome.components.zephyr_ble_server import DOMAIN as ZEPHYR_BLE_SERVER_DOMAIN
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_ERROR, Framework
from esphome.core import CoroPriority, coroutine_with_priority
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@omersiar"]
DEPENDENCIES = ["zephyr", "zephyr_ble_server"]

AdafruitBLEOTAComponent = cg.esphome_ns.namespace("adafruit_ble").class_(
    "OTAComponent", OTAComponent
)

# The device reboots into the bootloader before the transfer starts, so only
# on_begin and on_state_change can fire.
_UNSUPPORTED_TRIGGERS = (CONF_ON_PROGRESS, CONF_ON_END, CONF_ON_ERROR, CONF_ON_ABORT)

CONFIG_SCHEMA = cv.All(
    BASE_OTA_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(AdafruitBLEOTAComponent),
            **{
                cv.Optional(key): cv.invalid(
                    f"'{key}' never fires with Adafruit BLE OTA: the bootloader "
                    "runs the update after the device restarts into it"
                )
                for key in _UNSUPPORTED_TRIGGERS
            },
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ZEPHYR),
)


def _final_validate(config: ConfigType) -> None:
    # Every Adafruit bootloader variant is named adafruit*
    bootloader = zephyr_data().get(KEY_BOOTLOADER, "")
    if not bootloader.startswith(BOOTLOADER_ADAFRUIT):
        raise cv.Invalid(
            f"Adafruit BLE OTA requires an Adafruit bootloader, but '{bootloader}' "
            "is configured. Set 'bootloader:' to an Adafruit one."
        )
    server = fv.full_config.get().get(ZEPHYR_BLE_SERVER_DOMAIN) or {}
    if CONF_ON_NUMERIC_COMPARISON_REQUEST not in server:
        _LOGGER.warning(
            "Adafruit BLE OTA without pairing lets any nearby device restart this "
            "one into the bootloader; add '%s' to '%s' to require pairing",
            CONF_ON_NUMERIC_COMPARISON_REQUEST,
            ZEPHYR_BLE_SERVER_DOMAIN,
        )


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    # The Adafruit bootloader serves the update itself; the component provides
    # the BLE service that asks the application to restart into it.
    var = cg.new_Pvariable(config[CONF_ID])
    # Registered before ota_to_code waits past safe_mode, so safe mode keeps the trigger
    await cg.register_component(var, config)
    await ota_to_code(var, config)
