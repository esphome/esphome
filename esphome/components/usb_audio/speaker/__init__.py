import esphome.codegen as cg
from esphome.components import speaker
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID
from esphome.types import ConfigType

from .. import (
    CONF_USB_AUDIO_ID,
    set_stream_limits,
    stream_info,
    stream_schema,
    usb_audio_ns,
)

USBAudioSpeaker = usb_audio_ns.class_("USBAudioSpeaker", speaker.Speaker, cg.Component)

CONF_WRITE_TIMEOUT = "write_timeout"

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
)


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
