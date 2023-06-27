import esphome.codegen as cg

from esphome.components import esp32_ble_tracker

AUTO_LOAD = ["esp32_ble_tracker"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

esp32_ble_client_ns = cg.esphome_ns.namespace("esp32_ble_client")
BLEClientBase = esp32_ble_client_ns.class_(
    "BLEClientBase", esp32_ble_tracker.ESPBTClient, cg.Component
)
