import esphome.codegen as cg
from esphome.components import socket
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.types import ConfigType

CODEOWNERS = ["@max246"]
AUTO_LOAD = ["camera", "socket"]
DEPENDENCIES = ["network"]

CONF_MAX_SESSIONS = "max_sessions"

rtsp_server_ns = cg.esphome_ns.namespace("rtsp_server")
RTSPServer = rtsp_server_ns.class_("RTSPServer", cg.Component)


def _consume_rtsp_server_sockets(config: ConfigType) -> ConfigType:
    """Register socket needs for the RTSP server.

    One listening socket plus one TCP-interleaved session socket per concurrent
    client (no separate UDP sockets are used for media delivery).
    """
    socket.consume_sockets(config[CONF_MAX_SESSIONS], "rtsp_server")(config)
    socket.consume_sockets(1, "rtsp_server", socket.SocketType.TCP_LISTEN)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RTSPServer),
            cv.Optional(CONF_PORT, default=554): cv.port,
            cv.Optional(CONF_MAX_SESSIONS, default=2): cv.int_range(min=1, max=4),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    _consume_rtsp_server_sockets,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add_define("USE_RTSP_SERVER", True)
    cg.add_define("MAX_RTSP_SESSIONS", config[CONF_MAX_SESSIONS])
