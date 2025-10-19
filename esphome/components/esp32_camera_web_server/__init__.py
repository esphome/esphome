import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE, CONF_PORT
from esphome.types import ConfigType

CODEOWNERS = ["@ayufan"]
AUTO_LOAD = ["camera"]
DEPENDENCIES = ["network"]
MULTI_CONF = True

esp32_camera_web_server_ns = cg.esphome_ns.namespace("esp32_camera_web_server")
CameraWebServer = esp32_camera_web_server_ns.class_("CameraWebServer", cg.Component)
Mode = esp32_camera_web_server_ns.enum("Mode")

MODES = {"STREAM": Mode.STREAM, "SNAPSHOT": Mode.SNAPSHOT}


def _consume_camera_web_server_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for camera web server."""
    from esphome.components import socket

    # Each camera web server instance needs 1 listening socket + 2 client connections
    sockets_needed = 3
    socket.consume_sockets(sockets_needed, "esp32_camera_web_server")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CameraWebServer),
            cv.Required(CONF_PORT): cv.port,
            cv.Required(CONF_MODE): cv.enum(MODES, upper=True),
        },
    ).extend(cv.COMPONENT_SCHEMA),
    _consume_camera_web_server_sockets,
)


async def to_code(config):
    server = cg.new_Pvariable(config[CONF_ID])
    cg.add(server.set_port(config[CONF_PORT]))
    cg.add(server.set_mode(config[CONF_MODE]))
    await cg.register_component(server, config)
