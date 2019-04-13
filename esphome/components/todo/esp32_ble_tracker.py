from esphome import config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SCAN_INTERVAL, ESP_PLATFORM_ESP32
from esphome.core import HexInt


ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

CONF_ESP32_BLE_ID = 'esp32_ble_id'
ESP32BLETracker = esphome_ns.class_('ESP32BLETracker', Component)
XiaomiSensor = esphome_ns.class_('XiaomiSensor', sensor.Sensor)
XiaomiDevice = esphome_ns.class_('XiaomiDevice')
XIAOMI_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(XiaomiSensor)
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(ESP32BLETracker),
    cv.Optional(CONF_SCAN_INTERVAL): cv.positive_time_period_seconds,
}).extend(cv.COMPONENT_SCHEMA)


def make_address_array(address):
    return [HexInt(i) for i in address.parts]


def to_code(config):
    rhs = App.make_esp32_ble_tracker()
    ble = Pvariable(config[CONF_ID], rhs)
    if CONF_SCAN_INTERVAL in config:
        cg.add(ble.set_scan_interval(config[CONF_SCAN_INTERVAL]))

    register_component(ble, config)


BUILD_FLAGS = '-DUSE_ESP32_BLE_TRACKER'
