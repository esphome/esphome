import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_BATTERY_LEVEL, CONF_HUMIDITY, CONF_MAC_ADDRESS, CONF_TEMPERATURE, \
    UNIT_CELSIUS, ICON_THERMOMETER, UNIT_PERCENT, ICON_WATER_PERCENT, ICON_BATTERY, CONF_ID, \
    CONF_MOISTURE, CONF_ILLUMINANCE, CONF_CONDUCTIVITY, UNIT_LUX, ICON_BRIGHTNESS_5, \
    UNIT_MICROSIEMENS_PER_CENTIMETER, ICON_FLOWER, UNIT_KILOGRAM, ICON_SCALE, ICON_OMEGA, \
    UNIT_OHM, CONF_WEIGHT, CONF_IMPEDANCE
from .. import xiaomi_ble_ns

DEPENDENCIES = ['esp32_ble_tracker']

XiaomiSensor = xiaomi_ble_ns.class_('XiaomiSensor',
                                    esp32_ble_tracker.ESPBTDeviceListener, cg.Component)

SENSOR_TYPES = [
    CONF_TEMPERATURE, CONF_HUMIDITY, CONF_BATTERY_LEVEL, CONF_MOISTURE, CONF_ILLUMINANCE,
    CONF_CONDUCTIVITY, CONF_WEIGHT, CONF_IMPEDANCE
]
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(XiaomiSensor),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
    cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(UNIT_PERCENT, ICON_BATTERY, 0),
    cv.Optional(CONF_MOISTURE): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 0),
    cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(UNIT_LUX, ICON_BRIGHTNESS_5, 0),
    cv.Optional(CONF_CONDUCTIVITY):
        sensor.sensor_schema(UNIT_MICROSIEMENS_PER_CENTIMETER, ICON_FLOWER, 0),
    cv.Optional(CONF_WEIGHT): sensor.sensor_schema(UNIT_KILOGRAM, ICON_SCALE, 1),
    cv.Optional(CONF_IMPEDANCE): sensor.sensor_schema(UNIT_OHM, ICON_OMEGA, 1),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    for sensor_type in SENSOR_TYPES:
        if sensor_type not in config:
            continue

        sens = yield sensor.new_sensor(config[sensor_type])
        cg.add(getattr(var, 'set_{}'.format(sensor_type))(sens))
