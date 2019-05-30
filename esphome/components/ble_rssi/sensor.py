import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome.const import CONF_MAC_ADDRESS, CONF_ID, UNIT_DECIBEL, ICON_SIGNAL

DEPENDENCIES = ['esp32_ble_tracker']

ble_rssi_ns = cg.esphome_ns.namespace('ble_rssi')
BLERSSISensor = ble_rssi_ns.class_('BLERSSISensor', sensor.Sensor, cg.Component,
                                   esp32_ble_tracker.ESPBTDeviceListener)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_DECIBEL, ICON_SIGNAL, 0).extend({
    cv.GenerateID(): cv.declare_id(BLERSSISensor),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)
    yield sensor.register_sensor(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
