import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.esp32_ble import ESP32BLETracker
from esphomeyaml.const import CONF_MAC_ADDRESS, CONF_NAME, ESP_PLATFORM_ESP32
from esphomeyaml.core import HexInt
from esphomeyaml.helpers import ArrayInitializer, get_variable

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
DEPENDENCIES = ['esp32_ble']

CONF_ESP32_BLE_ID = 'esp32_ble_id'

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_variable_id(ESP32BLETracker)
}))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_ESP32_BLE_ID]):
        yield
    addr = [HexInt(i) for i in config[CONF_MAC_ADDRESS].parts]
    rhs = hub.make_device(config[CONF_NAME], ArrayInitializer(*addr, multiline=False))
    binary_sensor.register_binary_sensor(rhs, config)


BUILD_FLAGS = '-DUSE_ESP32_BLE_TRACKER'
