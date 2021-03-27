import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_BATTERY_VOLTAGE, CONF_HUMIDITY, CONF_MOISTURE, CONF_MAC_ADDRESS, CONF_TEMPERATURE, \
    UNIT_CELSIUS, ICON_THERMOMETER, UNIT_PERCENT, UNIT_VOLT, ICON_WATER_PERCENT, ICON_BATTERY, CONF_ID, \
    CONF_BINDKEY

CODEOWNERS = ['@rbaron']

DEPENDENCIES = ['esp32_ble_tracker']

b_parasite_ns = cg.esphome_ns.namespace('b_parasite')
BParasite = b_parasite_ns.class_('BParasite',
                                 esp32_ble_tracker.ESPBTDeviceListener,
                                 cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BParasite),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 3),
    cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_BATTERY, 3),
    cv.Optional(CONF_MOISTURE): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 3),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    for (config_key, setter) in [
        (CONF_TEMPERATURE, var.set_temperature),
        (CONF_HUMIDITY, var.set_humidity),
        (CONF_BATTERY_VOLTAGE, var.set_battery_voltage),
        (CONF_MOISTURE, var.set_soil_moisture),
    ]:
        if CONF_TEMPERATURE in config:
            sens = yield sensor.new_sensor(config[config_key])
            cg.add(setter(sens))
