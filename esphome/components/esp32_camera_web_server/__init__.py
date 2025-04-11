import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_PORT

CODEOWNERS = ["@ayufan"]
DEPENDENCIES = ["esp32_camera"]
MULTI_CONF = True

esp32_camera_web_server_ns = cg.esphome_ns.namespace("esp32_camera_web_server")
CameraWebServer = esp32_camera_web_server_ns.class_("CameraWebServer", cg.Component)
Mode = esp32_camera_web_server_ns.enum("Mode")

MODES = {"STREAM": Mode.STREAM, "SNAPSHOT": Mode.SNAPSHOT}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CameraWebServer),
        cv.Required(CONF_PORT): cv.port,
        cv.Required(CONF_MODE): cv.enum(MODES, upper=True),
    },
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    server = cg.new_Pvariable(config[CONF_ID])
    cg.add(server.set_port(config[CONF_PORT]))
    cg.add(server.set_mode(config[CONF_MODE]))
    await cg.register_component(server, config)
