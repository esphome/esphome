"""Test-only fake Daikin BRC1H (Madoka): answers the Madoka GATT protocol from an esp32_ble_server service."""

import esphome.codegen as cg
from esphome.components import esp32_ble
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DEPENDENCIES = ["esp32_ble_server"]
DOMAIN = "madoka_fake"
madoka_fake_ns = cg.esphome_ns.namespace("madoka_fake")
MadokaFake = madoka_fake_ns.class_("MadokaFake", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MadokaFake),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    esp32_ble.register_gap_event_handler(parent, var)
