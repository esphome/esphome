import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, ESP_PLATFORM_ESP32, CONF_PORT, CONF_MODE
from esphome.core import coroutine_with_priority

DEPENDENCIES = ["esp32_camera"]
CODEOWNERS = ["@ayufan"]

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

esp32_camera_web_server_ns = cg.esphome_ns.namespace("esp32_camera_web_server")
WebServer = esp32_camera_web_server_ns.class_("WebServer", cg.Component)
Mode = esp32_camera_web_server_ns.enum("Mode")

MODES = {"stream": Mode.Stream, "snapshot": Mode.Snapshot}

WEB_SERVER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WebServer),
        cv.Required(CONF_PORT): cv.port,
        cv.Required(CONF_MODE): cv.enum(MODES),
    }
).extend(cv.COMPONENT_SCHEMA)

CONFIG_SCHEMA = cv.ensure_list(WEB_SERVER_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    for _, conf in enumerate(config):
        server = cg.new_Pvariable(conf[CONF_ID])
        cg.add(server.set_port(conf[CONF_PORT]))
        cg.add(server.set_mode(conf[CONF_MODE]))
        yield cg.register_component(server, conf)
