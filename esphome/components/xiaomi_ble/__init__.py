import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32_ble_tracker import CONF_ESP32_BLE_ID, ESPBTDeviceListener, \
    ESP_BLE_DEVICE_SCHEMA
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32_ble_tracker']

xiaomi_ble_ns = cg.esphome_ns.namespace('xiaomi_ble')
XiaomiListener = xiaomi_ble_ns.class_('XiaomiListener', cg.Component, ESPBTDeviceListener)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(XiaomiListener),
}).extend(ESP_BLE_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    hub = yield cg.get_variable(config[CONF_ESP32_BLE_ID])
    var = cg.new_Pvariable(config[CONF_ID], hub)
    yield cg.register_component(var, config)
