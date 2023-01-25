import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option, get_esp32_variant, const

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@jesserockz"]
CONFLICTS_WITH = ["esp32_ble_beacon"]

CONF_BLE_ID = "ble_id"

NO_BLUTOOTH_VARIANTS = [const.VARIANT_ESP32S2]

esp32_ble_ns = cg.esphome_ns.namespace("esp32_ble")
ESP32BLE = esp32_ble_ns.class_("ESP32BLE", cg.Component)

GAPEventHandler = esp32_ble_ns.class_("GAPEventHandler")
GATTcEventHandler = esp32_ble_ns.class_("GATTcEventHandler")
GATTsEventHandler = esp32_ble_ns.class_("GATTsEventHandler")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLE),
    }
).extend(cv.COMPONENT_SCHEMA)


def validate_variant(_):
    variant = get_esp32_variant()
    if variant in NO_BLUTOOTH_VARIANTS:
        raise cv.Invalid(f"{variant} does not support Bluetooth")


FINAL_VALIDATE_SCHEMA = validate_variant


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
