import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESP32BLETracker, \
    make_address_array
from esphomeyaml.const import CONF_MAC_ADDRESS, CONF_NAME
from esphomeyaml.helpers import get_variable

DEPENDENCIES = ['esp32_ble_tracker']

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_variable_id(ESP32BLETracker)
}))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_ESP32_BLE_ID]):
        yield
    rhs = hub.make_rssi_sensor(config[CONF_NAME], make_address_array(config[CONF_MAC_ADDRESS]))
    sensor.register_sensor(rhs, config)
