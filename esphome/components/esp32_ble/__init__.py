import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODEL, ESP_PLATFORM_ESP32

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CODEOWNERS = ["@jesserockz"]

CONF_MANUFACTURER = "manufacturer"
CONF_SERVER = "server"

esp32_ble_ns = cg.esphome_ns.namespace("esp32_ble")
ESP32BLE = esp32_ble_ns.class_("ESP32BLE", cg.Component)
BLEServer = esp32_ble_ns.class_("BLEServer", cg.Component)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLE),
        cv.Optional(CONF_SERVER): cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(BLEServer),
                cv.Optional(CONF_MANUFACTURER, default="ESPHome"): cv.string,
                cv.Optional(CONF_MODEL): cv.string,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_SERVER in config:
        conf = config[CONF_SERVER]
        server = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(server, conf)
        cg.add(server.set_manufacturer(conf[CONF_MANUFACTURER]))
        if CONF_MODEL in conf:
            cg.add(server.set_model(conf[CONF_MODEL]))
        cg.add_define("USE_ESP32_BLE_SERVER")
        cg.add(var.set_server(server))
