import esphome.codegen as cg
from esphome.components import socket
from esphome.components.esp32_camera import (
    CONF_PIXEL_FORMAT,
    DOMAIN as ESP32_CAMERA_DOMAIN,
    FRAME_SIZES,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAX_CONNECTIONS, CONF_PORT, CONF_RESOLUTION
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@max246"]
AUTO_LOAD = ["camera", "socket"]
DEPENDENCIES = ["network"]
DOMAIN = "rtsp_server"

# RFC 2435 stores width and height in units of 8 pixels in one byte each.
RTP_JPEG_MAX_DIMENSION = 255 * 8

rtsp_server_ns = cg.esphome_ns.namespace("rtsp_server")
RTSPServer = rtsp_server_ns.class_("RTSPServer", cg.Component)


def _consume_rtsp_server_sockets(config: ConfigType) -> ConfigType:
    socket.consume_sockets(config[CONF_MAX_CONNECTIONS], DOMAIN)(config)
    socket.consume_sockets(1, DOMAIN, socket.SocketType.TCP_LISTEN)(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RTSPServer),
            cv.Optional(CONF_PORT, default=554): cv.port,
            cv.Optional(CONF_MAX_CONNECTIONS, default=2): cv.int_range(min=1, max=4),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _consume_rtsp_server_sockets,
)


def _final_validate(config: ConfigType) -> None:
    camera = fv.full_config.get().get(ESP32_CAMERA_DOMAIN)
    if camera is None:
        raise cv.Invalid(
            f"{DOMAIN} requires a camera, add an '{ESP32_CAMERA_DOMAIN}:' block"
        )
    if camera[CONF_PIXEL_FORMAT] != "JPEG":
        raise cv.Invalid(
            f"{DOMAIN} streams JPEG frames only, set '{CONF_PIXEL_FORMAT}: JPEG' "
            f"in '{ESP32_CAMERA_DOMAIN}:'"
        )
    resolution = camera[CONF_RESOLUTION]
    # The enum names end in the frame size, e.g. ESP32_CAMERA_SIZE_2560X1440
    width, height = map(int, str(FRAME_SIZES[resolution]).rsplit("_", 1)[1].split("X"))
    if max(width, height) > RTP_JPEG_MAX_DIMENSION:
        raise cv.Invalid(
            f"{DOMAIN} supports frames up to {RTP_JPEG_MAX_DIMENSION} pixels per side, "
            f"lower '{CONF_RESOLUTION}' in '{ESP32_CAMERA_DOMAIN}:' (got {width}x{height})"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PORT])
    await cg.register_component(var, config)
    cg.add_define("MAX_RTSP_CONNECTIONS", config[CONF_MAX_CONNECTIONS])
