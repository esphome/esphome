import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESPBTDeviceListener, \
    ESP_BLE_DEVICE_SCHEMA
from esphome.const import CONF_BATTERY_LEVEL, CONF_MAC_ADDRESS, CONF_TEMPERATURE, \
    CONF_UNIT_OF_MEASUREMENT, CONF_ICON, CONF_ACCURACY_DECIMALS, \
    UNIT_CELSIUS, ICON_THERMOMETER, UNIT_PERCENT, ICON_WATER_PERCENT, ICON_BATTERY, CONF_ID, \
    CONF_MOISTURE, CONF_ILLUMINANCE, ICON_BRIGHTNESS_5, UNIT_LUX, CONF_CONDUCTIVITY, \
    UNIT_MICROSIEMENS_PER_CENTIMETER, ICON_FLOWER

DEPENDENCIES = ['esp32_ble_tracker']
AUTO_LOAD = ['xiaomi_ble']

xiaomi_miflora_ns = cg.esphome_ns.namespace('xiaomi_miflora')
XiaomiMiflora = xiaomi_miflora_ns.class_('XiaomiMiflora', ESPBTDeviceListener, cg.Component)

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(XiaomiMiflora),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): sensor.unit_of_measurement,
        cv.Optional(CONF_ICON, default=ICON_THERMOMETER): sensor.icon,
        cv.Optional(CONF_ACCURACY_DECIMALS, default=1): sensor.accuracy_decimals
    })),
    cv.Optional(CONF_MOISTURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_PERCENT): sensor.unit_of_measurement,
        cv.Optional(CONF_ICON, default=ICON_WATER_PERCENT): sensor.icon,
        cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals
    })),
    cv.Optional(CONF_ILLUMINANCE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_LUX): sensor.unit_of_measurement,
        cv.Optional(CONF_ICON, default=ICON_BRIGHTNESS_5): sensor.icon,
        cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals
    })),
    cv.Optional(CONF_CONDUCTIVITY): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_MICROSIEMENS_PER_CENTIMETER):
            sensor.unit_of_measurement,
        cv.Optional(CONF_ICON, default=ICON_FLOWER): sensor.icon,
        cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals
    })),
    cv.Optional(CONF_BATTERY_LEVEL): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_PERCENT): sensor.unit_of_measurement,
        cv.Optional(CONF_ICON, default=ICON_BATTERY): sensor.icon,
        cv.Optional(CONF_ACCURACY_DECIMALS, default=0): sensor.accuracy_decimals
    })),
}).extend(ESP_BLE_DEVICE_SCHEMA)


def to_code(config):
    hub = yield cg.get_variable(config[CONF_ESP32_BLE_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub, config[CONF_MAC_ADDRESS].as_hex)
    yield cg.register_component(var, config)

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
    if CONF_MOISTURE in config:
        sens = yield sensor.new_sensor(config[CONF_MOISTURE])
        cg.add(var.set_moisture(sens))
    if CONF_ILLUMINANCE in config:
        sens = yield sensor.new_sensor(config[CONF_ILLUMINANCE])
        cg.add(var.set_illuminance(sens))
    if CONF_CONDUCTIVITY in config:
        sens = yield sensor.new_sensor(config[CONF_CONDUCTIVITY])
        cg.add(var.set_conductivity(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
