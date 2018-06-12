import voluptuous as vol

from esphomeyaml import config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_SCAN_INTERVAL, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

ESP32BLETracker = esphomelib_ns.ESP32BLETracker

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(ESP32BLETracker),
    vol.Optional(CONF_SCAN_INTERVAL): cv.positive_time_period_milliseconds,
})


def to_code(config):
    rhs = App.make_esp32_ble_tracker()
    ble = Pvariable(config[CONF_ID], rhs)
    if CONF_SCAN_INTERVAL in config:
        add(ble.set_scan_interval(config[CONF_SCAN_INTERVAL]))


BUILD_FLAGS = '-DUSE_ESP32_BLE_TRACKER'
