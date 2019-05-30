import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, esp32_ble_tracker
from esphome.const import CONF_MAC_ADDRESS, CONF_ID

DEPENDENCIES = ['esp32_ble_tracker']

ble_presence_ns = cg.esphome_ns.namespace('ble_presence')
BLEPresenceDevice = ble_presence_ns.class_('BLEPresenceDevice', binary_sensor.BinarySensor,
                                           cg.Component, esp32_ble_tracker.ESPBTDeviceListener)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BLEPresenceDevice),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
