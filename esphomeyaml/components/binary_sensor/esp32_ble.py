import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.esp32_ble import ESP32BLETracker
from esphomeyaml.const import CONF_MAC_ADDRESS, CONF_NAME, ESP_PLATFORM_ESP32
from esphomeyaml.core import HexInt, MACAddress
from esphomeyaml.helpers import ArrayInitializer, get_variable

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
DEPENDENCIES = ['esp32_ble']


def validate_mac(value):
    value = cv.string_strict(value)
    parts = value.split(':')
    if len(parts) != 6:
        raise vol.Invalid("MAC Address must consist of 6 : (colon) separated parts")
    parts_int = []
    if any(len(part) != 2 for part in parts):
        raise vol.Invalid("MAC Address must be format XX:XX:XX:XX:XX:XX")
    for part in parts:
        try:
            parts_int.append(int(part, 16))
        except ValueError:
            raise vol.Invalid("MAC Address parts must be hexadecimal values from 00 to FF")

    return MACAddress(*parts_int)


PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    vol.Required(CONF_MAC_ADDRESS): validate_mac,
}).extend(binary_sensor.BINARY_SENSOR_SCHEMA.schema)


def to_code(config):
    hub = get_variable(None, type=ESP32BLETracker)
    addr = [HexInt(i) for i in config[CONF_MAC_ADDRESS].parts]
    rhs = hub.make_device(config[CONF_NAME], ArrayInitializer(*addr, multiline=False))
    binary_sensor.register_binary_sensor(rhs, config)


BUILD_FLAGS = '-DUSE_ESP32_BLE_TRACKER'
