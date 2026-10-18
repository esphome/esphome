from collections.abc import Callable

import esphome.codegen as cg
from esphome.components import audio, esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_BITS_PER_SAMPLE,
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_PLATFORM,
    CONF_SAMPLE_RATE,
)
from esphome.core import CORE
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

# The full-speed USB controller of these variants shares one FIFO of 256 4-byte lines between received packets,
# control and bulk OUT packets, and isochronous OUT packets, and rejects an endpoint whose max packet size does not
# fit its share. ESP-IDF's default split leaves isochronous OUT 128 bytes, less than one packet of a 48 kHz 16-bit
# stereo speaker, so the split is sized from the configured formats instead.
FULL_SPEED_VARIANTS = (esp32.VARIANT_ESP32S2, esp32.VARIANT_ESP32S3)
FIFO_LINES = 256
FIFO_LINE_BYTES = 4
# Control and bulk packets are at most 64 bytes at full speed.
FIFO_NPTX_LINES = 16
# The shares ESP-IDF's default split leaves for other devices: 128 bytes each way. The controller reserves 2 of
# the RX lines for status.
FIFO_PTX_MIN_LINES = 32
FIFO_RX_STATUS_LINES = 2
FIFO_RX_MIN_LINES = 32 + FIFO_RX_STATUS_LINES
# usb_host_config_t::fifo_settings_custom
FIFO_CUSTOM_MIN_IDF = cv.Version(5, 5, 0)


def _final_validate(config: ConfigType) -> ConfigType:
    if (
        fifo_split(fv.full_config.get()) is not None
        and esp32.idf_version() < FIFO_CUSTOM_MIN_IDF
    ):
        raise cv.Invalid(
            f"usb_audio on {esp32.get_esp32_variant()} needs ESP-IDF {FIFO_CUSTOM_MIN_IDF} or later to size the USB FIFO"
        )
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBAudioComponent),
    }
).extend(cv.COMPONENT_SCHEMA)
FINAL_VALIDATE_SCHEMA = _final_validate


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

    def validator(config: ConfigType) -> ConfigType:
        hub_id = str(config[CONF_USB_AUDIO_ID])
        count = sum(
            1
            for platform_config in fv.full_config.get().get(domain, [])
            if platform_config.get(CONF_PLATFORM) == DOMAIN
            and str(platform_config.get(CONF_USB_AUDIO_ID)) == hub_id
        )
        if count > 1:
            raise cv.Invalid(
                f"Only one usb_audio {domain} per usb_audio component is supported",
                path=[CONF_USB_AUDIO_ID],
            )
        return config

    return validator


def max_packet_bytes(config: ConfigType) -> int:
    """Largest isochronous packet of a platform's format: a 1 ms packet, plus one frame for rate adaptation."""
    return (-(-config[CONF_SAMPLE_RATE] // 1000) + 1) * bytes_per_frame(config)


def _lines(size: int) -> int:
    return -(-size // FIFO_LINE_BYTES)


def _platform(full_config: ConfigType, domain: str) -> ConfigType | None:
    # The hub is not MULTI_CONF and takes one platform of each kind, so there is at most one.
    return next(
        (
            platform_config
            for platform_config in full_config.get(domain, [])
            if platform_config.get(CONF_PLATFORM) == DOMAIN
        ),
        None,
    )


def fifo_split(full_config: ConfigType) -> tuple[int, int, int] | None:
    """The (RX, non-periodic TX, periodic TX) FIFO lines for the configured streams on a full-speed controller.

    None when the variant's controller is not full speed or no stream is configured, so the default split applies.
    """
    if esp32.get_esp32_variant() not in FULL_SPEED_VARIANTS:
        return None
    speaker = _platform(full_config, "speaker")
    microphone = _platform(full_config, "microphone")
    if speaker is None and microphone is None:
        return None
    ptx = FIFO_PTX_MIN_LINES
    rx = FIFO_RX_MIN_LINES
    if speaker is not None:
        ptx = max(ptx, _lines(max_packet_bytes(speaker)))
    if microphone is not None:
        rx = max(rx, _lines(max_packet_bytes(microphone)) + FIFO_RX_STATUS_LINES)
    spare = FIFO_LINES - FIFO_NPTX_LINES - ptx - rx
    if spare < 0:
        raise cv.Invalid(
            f"The usb_audio formats need {(ptx + rx + FIFO_NPTX_LINES) * FIFO_LINE_BYTES} bytes of USB FIFO, but "
            f"the {esp32.get_esp32_variant()} has {FIFO_LINES * FIFO_LINE_BYTES}; lower the sample rate, bits per "
            "sample or channels"
        )
    # A device can declare a larger max packet size than the format needs, so the streams share the rest.
    if speaker is not None and microphone is not None:
        ptx += spare // 2
        rx += spare - spare // 2
    elif speaker is not None:
        ptx += spare
    else:
        rx += spare
    return rx, FIFO_NPTX_LINES, ptx


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
    if (split := fifo_split(CORE.config)) is not None:
        rx, nptx, ptx = split
        # Applied by usb_host when it installs the USB host library.
        cg.add_define("USB_HOST_FIFO_RX_LINES", rx)
        cg.add_define("USB_HOST_FIFO_NPTX_LINES", nptx)
        cg.add_define("USB_HOST_FIFO_PTX_LINES", ptx)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_define("USE_USB_AUDIO")
