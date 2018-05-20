import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_SCAN_INTERVAL, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID('esp32_ble'): cv.register_variable_id,
    vol.Optional(CONF_SCAN_INTERVAL): cv.positive_time_period_milliseconds,
})

ESP32BLETracker = esphomelib_ns.ESP32BLETracker


def to_code(config):
    rhs = App.make_esp32_ble_tracker()
    ble = Pvariable(ESP32BLETracker, config[CONF_ID], rhs)
    if CONF_SCAN_INTERVAL in config:
        add(ble.set_scan_interval(config[CONF_SCAN_INTERVAL]))


BUILD_FLAGS = '-DUSE_ESP32_BLE_TRACKER'
