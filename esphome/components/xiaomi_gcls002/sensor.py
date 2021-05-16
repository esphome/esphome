import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_MAC_ADDRESS,
    CONF_TEMPERATURE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    ICON_WATER_PERCENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    CONF_ID,
    CONF_MOISTURE,
    CONF_ILLUMINANCE,
    UNIT_LUX,
    CONF_CONDUCTIVITY,
    UNIT_MICROSIEMENS_PER_CENTIMETER,
    ICON_FLOWER,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_gcls002_ns = cg.esphome_ns.namespace("xiaomi_gcls002")
XiaomiGCLS002 = xiaomi_gcls002_ns.class_(
    "XiaomiGCLS002", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiGCLS002),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE
            ),
            cv.Optional(CONF_MOISTURE): sensor.sensor_schema(
                UNIT_PERCENT, ICON_WATER_PERCENT, 0, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                UNIT_LUX, ICON_EMPTY, 0, DEVICE_CLASS_ILLUMINANCE
            ),
            cv.Optional(CONF_CONDUCTIVITY): sensor.sensor_schema(
                UNIT_MICROSIEMENS_PER_CENTIMETER, ICON_FLOWER, 0, DEVICE_CLASS_EMPTY
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

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
