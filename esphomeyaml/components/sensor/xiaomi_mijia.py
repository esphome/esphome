import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESP32BLETracker, \
    make_address_array
from esphomeyaml.const import CONF_BATTERY_LEVEL, CONF_HUMIDITY, CONF_MAC_ADDRESS, CONF_MAKE_ID, \
    CONF_NAME, CONF_TEMPERATURE
from esphomeyaml.helpers import Pvariable, esphomelib_ns, get_variable

DEPENDENCIES = ['esp32_ble_tracker']

XiaomiMiJiaDevice = esphomelib_ns.XiaomiMiJiaDevice

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(XiaomiMiJiaDevice),
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_variable_id(ESP32BLETracker),
    vol.Required(CONF_MAC_ADDRESS): cv.mac_address,
    vol.Required(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Required(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_BATTERY_LEVEL): cv.nameable(sensor.SENSOR_SCHEMA),
})


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_ESP32_BLE_ID]):
        yield
    rhs = hub.make_mijia_sensor(config[CONF_TEMPERATURE][CONF_NAME],
                                config[CONF_HUMIDITY][CONF_NAME],
                                make_address_array(config[CONF_MAC_ADDRESS]))
    dev = Pvariable(config[CONF_MAKE_ID], rhs)
    sensor.register_sensor(dev.Pget_temperature_sensor(), config[CONF_TEMPERATURE])
    sensor.register_sensor(dev.Pget_humidity_sensor(), config[CONF_HUMIDITY])
    if CONF_BATTERY_LEVEL in config:
        conf = config[CONF_BATTERY_LEVEL]
        sensor.register_sensor(dev.Pmake_battery_level_sensor(conf[CONF_NAME]), conf)
