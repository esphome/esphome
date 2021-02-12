import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, ESP_PLATFORM_ESP32


ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CODEOWNERS = ['@jesserockz']

esp32_ble_server_ns = cg.esphome_ns.namespace('esp32_ble_server')
ESP32BLEServer = esp32_ble_server_ns.class_('ESP32BLEServer', cg.Component)


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ESP32BLEServer),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add_define('USE_ESP32_BLE_SERVER')
