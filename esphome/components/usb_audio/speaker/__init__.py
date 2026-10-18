import esphome.codegen as cg
from esphome.components import speaker
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_SAMPLE_RATE
from esphome.types import ConfigType

from .. import (
    CONF_USB_AUDIO_ID,
    bytes_per_frame,
    final_validate_one_per_hub,
    set_stream_limits,
    stream_info,
    stream_schema,
    usb_audio_ns,
)

USBAudioSpeaker = usb_audio_ns.class_("USBAudioSpeaker", speaker.Speaker, cg.Component)

CONF_WRITE_TIMEOUT = "write_timeout"

# Keep in sync with the usb_host_uac default of CONFIG_UAC_NUM_PACKETS_PER_URB (1 ms packets per transfer)
PACKETS_PER_TRANSFER = 3


def validate_buffer_size(config: ConfigType) -> ConfigType:
    """The driver only sends complete transfers, so the buffer must hold two of them to keep audio flowing."""
    frames_per_packet = -(-config[CONF_SAMPLE_RATE] // 1000)
    transfer_size = PACKETS_PER_TRANSFER * frames_per_packet * bytes_per_frame(config)
    min_size = 2 * transfer_size
    if config[CONF_BUFFER_SIZE] < min_size:
        raise cv.Invalid(
            f"buffer_size must be at least {min_size} bytes (two USB transfers) for this format",
            path=[CONF_BUFFER_SIZE],
        )
    return config


CONFIG_SCHEMA = cv.All(
    speaker.SPEAKER_SCHEMA.extend(stream_schema(48000, 2)).extend(
        {
            cv.GenerateID(): cv.declare_id(USBAudioSpeaker),
            cv.Optional(
                CONF_WRITE_TIMEOUT, default="20ms"
            ): cv.positive_time_period_milliseconds,
        }
    ),
    set_stream_limits,
    validate_buffer_size,
)

FINAL_VALIDATE_SCHEMA = final_validate_one_per_hub("speaker")


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_ID],
        stream_info(config),
        config[CONF_BUFFER_SIZE],
        config[CONF_WRITE_TIMEOUT].total_milliseconds,
    )
    await cg.register_component(var, config)
    await speaker.register_speaker(var, config)
    hub = await cg.get_variable(config[CONF_USB_AUDIO_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.register_speaker(var))
