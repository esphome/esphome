import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESP32BLETracker, \
    make_address_array
from esphomeyaml.const import CONF_BATTERY_LEVEL, CONF_CONDUCTIVITY, CONF_ILLUMINANCE, \
    CONF_MAC_ADDRESS, CONF_MAKE_ID, CONF_MOISTURE, CONF_NAME, CONF_TEMPERATURE
from esphomeyaml.helpers import Pvariable, esphomelib_ns, get_variable

DEPENDENCIES = ['esp32_ble_tracker']

XiaomiMiFloraDevice = esphomelib_ns.XiaomiMiFloraDevice

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(XiaomiMiFloraDevice),
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_variable_id(ESP32BLETracker),
    vol.Required(CONF_MAC_ADDRESS): cv.mac_address,
    vol.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_MOISTURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_ILLUMINANCE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_CONDUCTIVITY): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_BATTERY_LEVEL): cv.nameable(sensor.SENSOR_SCHEMA),
})


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_ESP32_BLE_ID]):
        yield
    rhs = hub.make_miflora_sensor(make_address_array(config[CONF_MAC_ADDRESS]))
    dev = Pvariable(config[CONF_MAKE_ID], rhs)
    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sensor.register_sensor(dev.Pmake_temperature_sensor(conf[CONF_NAME]), conf)
    if CONF_MOISTURE in config:
        conf = config[CONF_MOISTURE]
        sensor.register_sensor(dev.Pmake_moisture_sensor(conf[CONF_NAME]), conf)
    if CONF_ILLUMINANCE in config:
        conf = config[CONF_ILLUMINANCE]
        sensor.register_sensor(dev.Pmake_illuminance_sensor(conf[CONF_NAME]), conf)
    if CONF_CONDUCTIVITY in config:
        conf = config[CONF_CONDUCTIVITY]
        sensor.register_sensor(dev.Pmake_conductivity_sensor(conf[CONF_NAME]), conf)
    if CONF_BATTERY_LEVEL in config:
        conf = config[CONF_BATTERY_LEVEL]
        sensor.register_sensor(dev.Pmake_battery_level_sensor(conf[CONF_NAME]), conf)
