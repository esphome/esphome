import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESP_BLE_DEVICE_SCHEMA, \
    ESPBTDeviceListener
from esphome.const import CONF_MAC_ADDRESS, CONF_NAME, CONF_ID

DEPENDENCIES = ['esp32_ble_tracker']

ble_presence_ns = cg.esphome_ns.namespace('ble_presence')
BLEPresenceDevice = ble_presence_ns.class_('BLEPresenceDevice', binary_sensor.BinarySensor,
                                           cg.Component, ESPBTDeviceListener)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BLEPresenceDevice),
    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
}).extend(ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    hub = yield cg.get_variable(config[CONF_ESP32_BLE_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], hub, config[CONF_MAC_ADDRESS].as_hex)
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)
