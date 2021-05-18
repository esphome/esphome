import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL, ESP_PLATFORM_ESP32
from esphome.core import coroutine_with_priority


ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CODEOWNERS = ["@jesserockz"]

CONF_MANUFACTURER = "manufacturer"

esp32_ble_server_ns = cg.esphome_ns.namespace("esp32_ble_server")
ESP32BLEServer = esp32_ble_server_ns.class_("ESP32BLEServer", cg.Component)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLEServer),
        cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.string,
        cv.Optional(CONF_MODEL): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(60.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    cg.add(var.set_manufacturer(config[CONF_MANUFACTURER]))
    if CONF_MODEL in config:
        cg.add(var.set_model(config[CONF_MODEL]))
    cg.add_define("USE_ESP32_BLE_SERVER")
