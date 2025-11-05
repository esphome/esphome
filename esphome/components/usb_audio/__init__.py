import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@ab-tools"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["audio"]
MULTI_CONF = False

usb_audio_ns = cg.esphome_ns.namespace("usb_audio")
USBAudioComponent = usb_audio_ns.class_("USBAudioComponent", cg.Component)

CONF_CONNECT_TIMEOUT = "connect_timeout"
CONF_USB_AUDIO = "usb_audio"
CONF_USB_AUDIO_ID = "usb_audio_id"
CONF_MIC_BUFFER_SIZE = "microphone_buffer_size"
CONF_SPEAKER_BUFFER_SIZE = "speaker_buffer_size"
CONF_DEFAULT_BUFFER_SIZE = 6400

SUPPORTED_VARIANTS = [
    esp32.const.VARIANT_ESP32S2,
    esp32.const.VARIANT_ESP32S3,
    esp32.const.VARIANT_ESP32P4,
]


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(USBAudioComponent),
            cv.Optional(
                CONF_CONNECT_TIMEOUT, default="5000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_MIC_BUFFER_SIZE, default=CONF_DEFAULT_BUFFER_SIZE
            ): cv.positive_int,
            cv.Optional(
                CONF_SPEAKER_BUFFER_SIZE, default=CONF_DEFAULT_BUFFER_SIZE
            ): cv.positive_int,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
    esp32.only_on_variant(supported=SUPPORTED_VARIANTS),
)


def _ensure_buffer_size(value: int) -> int:
    if value <= 0:
        return CONF_DEFAULT_BUFFER_SIZE
    return value


async def to_code(config):
    esp32.add_idf_component(name="espressif/usb_host_uac", ref="1.3.1")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_USB_AUDIO")

    connect_timeout = config[CONF_CONNECT_TIMEOUT].total_milliseconds
    cg.add(var.set_connect_timeout(connect_timeout))
    cg.add(
        var.set_microphone_buffer_size(
            _ensure_buffer_size(config[CONF_MIC_BUFFER_SIZE])
        )
    )
    cg.add(
        var.set_speaker_buffer_size(
            _ensure_buffer_size(config[CONF_SPEAKER_BUFFER_SIZE])
        )
    )
