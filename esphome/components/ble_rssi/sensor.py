import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESPBTDeviceListener, \
    ESP_BLE_DEVICE_SCHEMA
from esphome.const import CONF_MAC_ADDRESS, CONF_NAME, CONF_ID, UNIT_DECIBEL, ICON_SIGNAL

DEPENDENCIES = ['esp32_ble_tracker']

ble_rssi_ns = cg.esphome_ns.namespace('ble_rssi')
BLERSSISensor = ble_rssi_ns.class_('BLERSSISensor', sensor.Sensor, cg.Component,
                                   ESPBTDeviceListener)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_DECIBEL, ICON_SIGNAL, 0).extend({
    cv.GenerateID(): cv.declare_id(BLERSSISensor),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
}).extend(ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    hub = yield cg.get_variable(config[CONF_ESP32_BLE_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], hub, config[CONF_MAC_ADDRESS].as_hex)
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
