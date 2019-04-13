from esphome.components import esp32_ble_tracker, sensor
from esphome.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESP32BLETracker, \
    make_address_array
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BATTERY_LEVEL, CONF_CONDUCTIVITY, CONF_ID, CONF_ILLUMINANCE, \
    CONF_MAC_ADDRESS, CONF_MOISTURE, CONF_NAME, CONF_TEMPERATURE
DEPENDENCIES = ['esp32_ble_tracker']

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(esp32_ble_tracker.XiaomiDevice),
    cv.GenerateID(CONF_ESP32_BLE_ID): cv.use_variable_id(ESP32BLETracker),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_TEMPERATURE): cv.nameable(esp32_ble_tracker.XIAOMI_SENSOR_SCHEMA),
    cv.Optional(CONF_MOISTURE): cv.nameable(esp32_ble_tracker.XIAOMI_SENSOR_SCHEMA),
    cv.Optional(CONF_ILLUMINANCE): cv.nameable(esp32_ble_tracker.XIAOMI_SENSOR_SCHEMA),
    cv.Optional(CONF_CONDUCTIVITY): cv.nameable(esp32_ble_tracker.XIAOMI_SENSOR_SCHEMA),
    cv.Optional(CONF_BATTERY_LEVEL): cv.nameable(esp32_ble_tracker.XIAOMI_SENSOR_SCHEMA),
})


def to_code(config):
    hub = yield get_variable(config[CONF_ESP32_BLE_ID])
    rhs = hub.make_xiaomi_device(make_address_array(config[CONF_MAC_ADDRESS]))
    dev = Pvariable(config[CONF_ID], rhs)
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
