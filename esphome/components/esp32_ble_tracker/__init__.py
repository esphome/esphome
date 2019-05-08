import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SCAN_INTERVAL, ESP_PLATFORM_ESP32
from esphome.core import coroutine

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
AUTO_LOAD = ['xiaomi_ble']

CONF_ESP32_BLE_ID = 'esp32_ble_id'
esp32_ble_tracker_ns = cg.esphome_ns.namespace('esp32_ble_tracker')
ESP32BLETracker = esp32_ble_tracker_ns.class_('ESP32BLETracker', cg.Component)
ESPBTDeviceListener = esp32_ble_tracker_ns.class_('ESPBTDeviceListener')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32BLETracker),
    cv.Optional(CONF_SCAN_INTERVAL, default='300s'): cv.positive_time_period_seconds,
}).extend(cv.COMPONENT_SCHEMA)

ESP_BLE_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_id(ESP32BLETracker),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add(var.set_scan_interval(config[CONF_SCAN_INTERVAL]))


@coroutine
def register_ble_device(var, config):
    paren = yield cg.get_variable(config[CONF_ESP32_BLE_ID])
    cg.add(paren.register_listener(var))
    yield var
