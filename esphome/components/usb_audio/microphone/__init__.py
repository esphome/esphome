import esphome.codegen as cg
from esphome.components import microphone
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_SAMPLE_RATE
from esphome.types import ConfigType

from .. import (
    CONF_USB_AUDIO_ID,
    apply_buffer_size,
    bytes_per_frame,
    final_validate_one_per_hub,
    set_stream_limits,
    stream_info,
    stream_schema,
    usb_audio_ns,
)

USBAudioMicrophone = usb_audio_ns.class_(
    "USBAudioMicrophone", microphone.Microphone, cg.Component
)

# Keep in sync with USBAudioMicrophone::READ_TIMEOUT_MS in usb_audio_microphone.h
READ_PERIOD_MS = 20


def validate_buffer_size(config: ConfigType) -> ConfigType:
    """The driver buffer must hold the audio that arrives between two reads, with room to spare."""
    bytes_per_second = config[CONF_SAMPLE_RATE] * bytes_per_frame(config)
    min_size = bytes_per_second * 2 * READ_PERIOD_MS // 1000
    return apply_buffer_size(config, min_size, f"{2 * READ_PERIOD_MS} ms of audio")


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(stream_schema(16000, 1)).extend(
        {
            cv.GenerateID(): cv.declare_id(USBAudioMicrophone),
        }
    ),
    set_stream_limits,
    validate_buffer_size,
)

FINAL_VALIDATE_SCHEMA = final_validate_one_per_hub("microphone")


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_ID], stream_info(config), config[CONF_BUFFER_SIZE]
    )
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)
    hub = await cg.get_variable(config[CONF_USB_AUDIO_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.register_microphone(var))
