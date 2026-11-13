import esphome.codegen as cg
from esphome.components import microphone, network, socket
from esphome.components.const import CONF_ALLOWED_IPS
from esphome.components.esp32_camera import (
    CONF_JPEG_QUALITY,
    CONF_PIXEL_FORMAT,
    DOMAIN as ESP32_CAMERA_DOMAIN,
    FRAME_SIZES,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_CONNECTIONS,
    CONF_MICROPHONE,
    CONF_PORT,
    CONF_RESOLUTION,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@max246"]
DEPENDENCIES = ["network"]
DOMAIN = "rtsp_server"

CONF_AUDIO_CODEC = "audio_codec"

# G.711 has static RTP payload types only at 8 kHz (RFC 3551 section 6).
G711_SAMPLE_RATE: int = 8000

# RFC 2435 stores width and height in units of 8 pixels in one byte each.
RTP_JPEG_MAX_DIMENSION = 255 * 8

rtsp_server_ns = cg.esphome_ns.namespace("rtsp_server")
RTSPServer = rtsp_server_ns.class_("RTSPServer", cg.Component)
AudioCodec = rtsp_server_ns.enum("AudioCodec", is_class=True)
AUDIO_CODECS: dict[str, cg.MockObj] = AudioCodec.options(
    "L16", "PCMU", "PCMA", prefix="AUDIO_CODEC_"
)


def AUTO_LOAD(config: ConfigType) -> list[str]:
    """The ring buffer carries microphone audio from its task to the main loop."""
    base = ["camera", "socket"]
    # A falsy config is a tooling probe for the maximal set
    if not config or CONF_MICROPHONE in config:
        return base + ["ring_buffer"]
    return base


def _validate_audio_codec(config: ConfigType) -> ConfigType:
    if CONF_AUDIO_CODEC in config and CONF_MICROPHONE not in config:
        raise cv.Invalid(f"'{CONF_AUDIO_CODEC}' needs a '{CONF_MICROPHONE}'")
    return config


def _consume_rtsp_server_sockets(config: ConfigType) -> ConfigType:
    socket.consume_sockets(config[CONF_MAX_CONNECTIONS], DOMAIN)(config)
    socket.consume_sockets(1, DOMAIN, socket.SocketType.TCP_LISTEN)(config)
    # A 13 KB frame fills the default 5.7 KB TCP send buffer four segments at a time; over Wi-Fi
    # that is about 1 fps, so ask for the larger buffers (network: enable_high_performance: false undoes it)
    network.require_high_performance_networking()
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RTSPServer),
            cv.Optional(CONF_PORT, default=554): cv.port,
            cv.Optional(CONF_MAX_CONNECTIONS, default=2): cv.int_range(min=1, max=4),
            cv.Optional(CONF_ALLOWED_IPS): socket.IPV4_ALLOW_SCHEMA,
            cv.Optional(CONF_MICROPHONE): cv.All(
                cv.only_on_esp32,
                microphone.microphone_source_schema(
                    min_bits_per_sample=16, max_bits_per_sample=16
                ),
            ),
            cv.Optional(CONF_AUDIO_CODEC): cv.enum(AUDIO_CODECS, upper=True),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_audio_codec,
    _consume_rtsp_server_sockets,
)


def _final_validate(config: ConfigType) -> None:
    camera = fv.full_config.get().get(ESP32_CAMERA_DOMAIN)
    if camera is None:
        raise cv.Invalid(
            f"{DOMAIN} requires a camera, add an '{ESP32_CAMERA_DOMAIN}:' block"
        )
    pixel_format = camera[CONF_PIXEL_FORMAT]
    # A GRAYSCALE frame converts to a one-component JPEG, which RTP/JPEG cannot carry.
    if pixel_format == "GRAYSCALE":
        raise cv.Invalid(
            f"{DOMAIN} cannot stream '{CONF_PIXEL_FORMAT}: GRAYSCALE', use a color "
            f"format in '{ESP32_CAMERA_DOMAIN}:'"
        )
    # Other formats are converted to JPEG only when jpeg_quality is set.
    if pixel_format != "JPEG" and camera[CONF_JPEG_QUALITY] == 0:
        raise cv.Invalid(
            f"{DOMAIN} streams JPEG frames, set '{CONF_JPEG_QUALITY}' in "
            f"'{ESP32_CAMERA_DOMAIN}:' to convert '{CONF_PIXEL_FORMAT}: {pixel_format}'"
        )
    resolution = camera[CONF_RESOLUTION]
    # The enum names end in the frame size, e.g. ESP32_CAMERA_SIZE_2560X1440
    width, height = map(int, str(FRAME_SIZES[resolution]).rsplit("_", 1)[1].split("X"))
    if max(width, height) > RTP_JPEG_MAX_DIMENSION:
        raise cv.Invalid(
            f"{DOMAIN} supports frames up to {RTP_JPEG_MAX_DIMENSION} pixels per side, "
            f"lower '{CONF_RESOLUTION}' in '{ESP32_CAMERA_DOMAIN}:' (got {width}x{height})"
        )
    if (mic := config.get(CONF_MICROPHONE)) is not None:
        g711 = config.get(CONF_AUDIO_CODEC, "L16") != "L16"
        microphone.final_validate_microphone_source_schema(
            DOMAIN, sample_rate=G711_SAMPLE_RATE if g711 else cv.UNDEFINED
        )(mic)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PORT])
    await cg.register_component(var, config)
    cg.add_define("MAX_RTSP_CONNECTIONS", config[CONF_MAX_CONNECTIONS])
    socket.add_ipv4_allow(var.set_allow, config.get(CONF_ALLOWED_IPS), config[CONF_ID])
    if (mic := config.get(CONF_MICROPHONE)) is not None:
        cg.add_define("USE_RTSP_SERVER_AUDIO")
        mic_source = await microphone.microphone_source_to_code(mic)
        cg.add(var.set_microphone_source(mic_source))
        cg.add(var.set_audio_codec(AUDIO_CODECS[config.get(CONF_AUDIO_CODEC, "L16")]))
