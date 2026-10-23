from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import audio, esp32, usb_host
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
)
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@ab-tools"]
DEPENDENCIES = ["esp32"]
# usb_host installs the USB host library, services its events and limits the variants
AUTO_LOAD = ["audio", "usb_host"]
DOMAIN = "usb_audio"

usb_audio_ns = cg.esphome_ns.namespace("usb_audio")
USBAudioComponent = usb_audio_ns.class_("USBAudioComponent", cg.Component)
AudioStreamInfo = audio.audio_ns.class_("AudioStreamInfo")

CONF_USB_AUDIO_ID = "usb_audio_id"
BITS_PER_SAMPLE_OPTIONS = [8, 16, 24, 32]
DEFAULT_BUFFER_SIZE = 6400

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBAudioComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


def stream_schema(default_sample_rate: int, default_num_channels: int) -> cv.Schema:
    """Stream format and buffer options shared by the speaker and microphone platforms."""
    return cv.Schema(
        {
            cv.GenerateID(CONF_USB_AUDIO_ID): cv.use_id(USBAudioComponent),
            cv.Optional(CONF_SAMPLE_RATE, default=default_sample_rate): cv.int_range(
                min=8000, max=96000
            ),
            cv.Optional(CONF_BITS_PER_SAMPLE, default="16bit"): cv.All(
                cv.float_with_unit("bits", "bit"), cv.one_of(*BITS_PER_SAMPLE_OPTIONS)
            ),
            cv.Optional(CONF_NUM_CHANNELS, default=default_num_channels): cv.int_range(
                min=1, max=2
            ),
            # The default depends on the format; see apply_buffer_size()
            cv.Optional(CONF_BUFFER_SIZE): cv.positive_not_null_int,
        }
    ).extend(cv.COMPONENT_SCHEMA)


def set_stream_limits(config: ConfigType) -> ConfigType:
    """Pin the audio stream limits to the one configured format."""
    bits_per_sample = int(config[CONF_BITS_PER_SAMPLE])
    channels = config[CONF_NUM_CHANNELS]
    sample_rate = config[CONF_SAMPLE_RATE]
    # The limits validator edits the config in place and returns None
    audio.set_stream_limits(
        min_bits_per_sample=bits_per_sample,
        max_bits_per_sample=bits_per_sample,
        min_channels=channels,
        max_channels=channels,
        min_sample_rate=sample_rate,
        max_sample_rate=sample_rate,
    )(config)
    return config


def bytes_per_frame(config: ConfigType) -> int:
    """Bytes in one frame (one sample for every channel) of a platform's configured format."""
    return config[CONF_NUM_CHANNELS] * int(config[CONF_BITS_PER_SAMPLE]) // 8


def apply_buffer_size(config: ConfigType, min_size: int, minimum: str) -> ConfigType:
    """Default buffer_size to DEFAULT_BUFFER_SIZE, or the format's minimum when that is larger; reject smaller ones.

    `minimum` describes what min_size holds, for the error message.
    """
    if CONF_BUFFER_SIZE not in config:
        config[CONF_BUFFER_SIZE] = max(DEFAULT_BUFFER_SIZE, min_size)
    elif config[CONF_BUFFER_SIZE] < min_size:
        raise cv.Invalid(
            f"buffer_size must be at least {min_size} bytes ({minimum}) for this format",
            path=[CONF_BUFFER_SIZE],
        )
    return config


def final_validate_one_per_hub(domain: str) -> Callable[[ConfigType], ConfigType]:
    """The hub streams one speaker and one microphone, so reject a second platform of the same kind on a hub."""
    return fv.unique_platform_key(
        domain,
        DOMAIN,
        CONF_USB_AUDIO_ID,
        message=f"Only one usb_audio {domain} per usb_audio component is supported",
    )


def max_packet_bytes(config: ConfigType) -> int:
    """Largest isochronous packet of a platform's format: a 1 ms packet, plus one frame for rate adaptation."""
    return (-(-config[CONF_SAMPLE_RATE] // 1000) + 1) * bytes_per_frame(config)


def request_speaker_fifo(config: ConfigType) -> ConfigType:
    """Ask usb_host for USB FIFO space for the speaker's isochronous OUT packets."""
    usb_host.request_periodic_fifo(out_bytes=max_packet_bytes(config))
    return config


def request_microphone_fifo(config: ConfigType) -> ConfigType:
    """Ask usb_host for USB FIFO space for the microphone's isochronous IN packets."""
    usb_host.request_periodic_fifo(in_bytes=max_packet_bytes(config))
    return config


def stream_info(config: ConfigType) -> MockObj:
    """The C++ AudioStreamInfo for a platform's configured format."""
    return AudioStreamInfo(
        int(config[CONF_BITS_PER_SAMPLE]),
        config[CONF_NUM_CHANNELS],
        config[CONF_SAMPLE_RATE],
    )


async def to_code(config: ConfigType) -> None:
    # Keep in sync with esphome/idf_component.yml. usb_host adds espressif/usb on IDF 6.0 and later.
    esp32.add_idf_component(name="espressif/usb_host_uac", ref="1.5.0")
    # 18 ms of transfers in flight instead of the driver's 9 ms, so a late transfer completion does not leave the bus
    # idle while the ring buffer still holds audio.
    esp32.set_idf_sdkconfig_default("CONFIG_UAC_NUM_ISOC_URBS", 6)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_USB_AUDIO")
