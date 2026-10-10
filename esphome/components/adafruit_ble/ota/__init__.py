import esphome.codegen as cg
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_BOOTLOADER
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@omersiar"]
DEPENDENCIES = ["zephyr"]

ZEPHYR_BLE_SERVER = "zephyr_ble_server"

AdafruitBLEOTAComponent = cg.esphome_ns.namespace("adafruit_ble").class_(
    "OTAComponent", OTAComponent
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AdafruitBLEOTAComponent),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework(Framework.ZEPHYR),
)


def _validate_ble_server() -> None:
    if ZEPHYR_BLE_SERVER not in CORE.loaded_integrations:
        raise cv.Invalid(
            f"'{ZEPHYR_BLE_SERVER}' component is required for Adafruit BLE OTA"
        )


def _validate_bootloader() -> None:
    # Every Adafruit bootloader variant is named adafruit*
    bootloader = zephyr_data().get(KEY_BOOTLOADER, "")
    if bootloader.startswith(BOOTLOADER_ADAFRUIT):
        return
    raise cv.Invalid(
        f"Adafruit BLE OTA requires an Adafruit bootloader, but '{bootloader}' is "
        "configured. Set 'bootloader:' to an Adafruit one."
    )


def _final_validate(config: ConfigType) -> None:
    _validate_ble_server()
    _validate_bootloader()


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config: ConfigType) -> None:
    # The Adafruit bootloader serves the update itself; the component provides
    # the BLE service that asks the application to restart into it.
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
