import esphome.codegen as cg
from esphome.components import camera_video
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_USERNAME
from esphome.types import ConfigType

AUTO_LOAD = ["camera_video", "socket"]
DEPENDENCIES = ["network", "esp32"]
CODEOWNERS = ["@jvgelder"]

rtsp_server_ns = cg.esphome_ns.namespace("rtsp_server")
RtspTrack = rtsp_server_ns.class_("RtspTrack")
H264RtspTrack = rtsp_server_ns.class_(
    "H264RtspTrack", RtspTrack, camera_video.H264StreamListener
)
RtspServer = rtsp_server_ns.class_("RtspServer", cg.Component)

CONF_STREAM_ID = "stream_id"
CONF_TRACK_ID = "track_id"
CONF_RTP_PAYLOAD_SIZE = "rtp_payload_size"

DEFAULT_RTSP_PORT = 554
DEFAULT_RTP_PAYLOAD_SIZE = 1400
MIN_RTP_PAYLOAD_SIZE = 256
MAX_RTP_PAYLOAD_SIZE = 1400
RTSP_MAX_CLIENTS = 2


def _validate_credentials(config: ConfigType) -> ConfigType:
    username = config[CONF_USERNAME]
    password = config[CONF_PASSWORD]
    if bool(username) != bool(password):
        raise cv.Invalid("username and password must be configured together")
    if len(f"{username}:{password}".encode()) >= 256:
        raise cv.Invalid("combined RTSP username/password must be at most 255 bytes")
    return config


def _consume_sockets(config: ConfigType) -> ConfigType:
    from esphome.components import socket

    socket.consume_sockets(1, "rtsp_server", socket.SocketType.TCP_LISTEN)(config)
    socket.consume_sockets(RTSP_MAX_CLIENTS, "rtsp_server")(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RtspServer),
            cv.GenerateID(CONF_TRACK_ID): cv.declare_id(H264RtspTrack),
            cv.Required(CONF_STREAM_ID): cv.use_id(camera_video.H264Stream),
            cv.Optional(CONF_PORT, default=DEFAULT_RTSP_PORT): cv.port,
            cv.Optional(CONF_USERNAME, default=""): cv.string_strict,
            cv.Optional(CONF_PASSWORD, default=""): cv.sensitive(cv.string_strict),
            cv.Optional(
                CONF_RTP_PAYLOAD_SIZE, default=DEFAULT_RTP_PAYLOAD_SIZE
            ): cv.int_range(MIN_RTP_PAYLOAD_SIZE, MAX_RTP_PAYLOAD_SIZE),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_credentials,
    _consume_sockets,
)


async def to_code(config: ConfigType) -> None:
    stream = await cg.get_variable(config[CONF_STREAM_ID])
    track = cg.new_Pvariable(config[CONF_TRACK_ID], stream)
    server = cg.new_Pvariable(config[CONF_ID], track)
    await cg.register_component(server, config)

    cg.add(server.set_port(config[CONF_PORT]))
    cg.add(server.set_auth_username(config[CONF_USERNAME]))
    cg.add(server.set_auth_password(config[CONF_PASSWORD]))
    cg.add(server.set_rtp_payload_size(config[CONF_RTP_PAYLOAD_SIZE]))
    cg.add_define("USE_RTSP_SERVER")
