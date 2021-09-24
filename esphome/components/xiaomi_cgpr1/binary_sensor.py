import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, binary_sensor, esp32_ble_tracker
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BINDKEY,
    CONF_DEVICE_CLASS,
    CONF_MAC_ADDRESS,
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_EMPTY,
    UNIT_PERCENT,
    CONF_IDLE_TIME,
    CONF_ILLUMINANCE,
    UNIT_MINUTE,
    UNIT_LUX,
    ICON_TIMELAPSE,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble", "sensor"]

xiaomi_cgpr1_ns = cg.esphome_ns.namespace("xiaomi_cgpr1")
XiaomiCGPR1 = xiaomi_cgpr1_ns.class_(
    "XiaomiCGPR1",
    binary_sensor.BinarySensor,
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.BINARY_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(XiaomiCGPR1),
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(
                CONF_DEVICE_CLASS, default="motion"
            ): binary_sensor.device_class,
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                UNIT_PERCENT, ICON_EMPTY, 0, DEVICE_CLASS_BATTERY
            ),
            cv.Optional(CONF_IDLE_TIME): sensor.sensor_schema(
                UNIT_MINUTE, ICON_TIMELAPSE, 0, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                UNIT_LUX, ICON_EMPTY, 0, DEVICE_CLASS_ILLUMINANCE
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
    yield binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_bindkey(config[CONF_BINDKEY]))

    if CONF_IDLE_TIME in config:
        sens = yield sensor.new_sensor(config[CONF_IDLE_TIME])
        cg.add(var.set_idle_time(sens))
    if CONF_BATTERY_LEVEL in config:
        sens = yield sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery_level(sens))
    if CONF_ILLUMINANCE in config:
        sens = yield sensor.new_sensor(config[CONF_ILLUMINANCE])
        cg.add(var.set_illuminance(sens))
