import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_MAC_ADDRESS,
    CONF_TEMPERATURE,
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_EMPTY,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    CONF_HUMIDITY,
    UNIT_MILLIGRAMS_PER_CUBIC_METER,
    ICON_FLASK_OUTLINE,
    CONF_FORMALDEHYDE,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble"]

xiaomi_jqjcy01ym_ns = cg.esphome_ns.namespace("xiaomi_jqjcy01ym")
XiaomiJQJCY01YM = xiaomi_jqjcy01ym_ns.class_(
    "XiaomiJQJCY01YM", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiJQJCY01YM),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                UNIT_CELSIUS, ICON_EMPTY, 1, DEVICE_CLASS_TEMPERATURE
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                UNIT_PERCENT, ICON_EMPTY, 0, DEVICE_CLASS_HUMIDITY
            ),
            cv.Optional(CONF_FORMALDEHYDE): sensor.sensor_schema(
                UNIT_MILLIGRAMS_PER_CUBIC_METER,
                ICON_FLASK_OUTLINE,
                2,
                DEVICE_CLASS_EMPTY,
            ),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                UNIT_PERCENT, ICON_EMPTY, 0, DEVICE_CLASS_BATTERY
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
    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
    if CONF_FORMALDEHYDE in config:
        sens = yield sensor.new_sensor(config[CONF_FORMALDEHYDE])
        cg.add(var.set_formaldehyde(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
