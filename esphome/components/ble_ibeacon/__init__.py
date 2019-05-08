import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_ID

DEPENDENCIES = ['esp32_ble_tracker']
ble_ibeacon_ns = cg.esphome_ns.namespace('ble_ibeacon')
IBeaconListener = ble_ibeacon_ns.class_('IBeaconListener', esp32_ble_tracker.ESPBTDeviceListener)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(IBeaconListener),
}).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield esp32_ble_tracker.register_ble_device(var, config)
