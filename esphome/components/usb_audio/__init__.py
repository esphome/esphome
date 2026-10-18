import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@ab-tools"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["audio"]
DOMAIN = "usb_audio"

usb_audio_ns = cg.esphome_ns.namespace("usb_audio")
USBAudioComponent = usb_audio_ns.class_("USBAudioComponent", cg.Component)

CONF_CONNECT_TIMEOUT = "connect_timeout"
CONF_USB_AUDIO_ID = "usb_audio_id"
CONF_MICROPHONE_BUFFER_SIZE = "microphone_buffer_size"
CONF_SPEAKER_BUFFER_SIZE = "speaker_buffer_size"
DEFAULT_BUFFER_SIZE = 6400

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
                CONF_MICROPHONE_BUFFER_SIZE, default=DEFAULT_BUFFER_SIZE
            ): cv.positive_not_null_int,
            cv.Optional(
                CONF_SPEAKER_BUFFER_SIZE, default=DEFAULT_BUFFER_SIZE
            ): cv.positive_not_null_int,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework("esp-idf"),
    esp32.only_on_variant(supported=SUPPORTED_VARIANTS),
)


async def to_code(config: ConfigType) -> None:
    # Keep in sync with esphome/idf_component.yml. Pulls in espressif/usb itself on IDF 6.0 and later.
    esp32.add_idf_component(name="espressif/usb_host_uac", ref="1.3.1")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_USB_AUDIO")
    # The hub is single-instance, so the buffers are sized at compile time.
    cg.add_define(
        "USB_AUDIO_MICROPHONE_BUFFER_SIZE", config[CONF_MICROPHONE_BUFFER_SIZE]
    )
    cg.add_define("USB_AUDIO_SPEAKER_BUFFER_SIZE", config[CONF_SPEAKER_BUFFER_SIZE])
    cg.add(var.set_connect_timeout(config[CONF_CONNECT_TIMEOUT].total_milliseconds))
