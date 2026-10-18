import esphome.codegen as cg
from esphome.components import microphone
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

USBAudioMicrophone = usb_audio_ns.class_(
    "USBAudioMicrophone", microphone.Microphone, cg.Component
)

CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(stream_schema(16000, 1)).extend(
        {
            cv.GenerateID(): cv.declare_id(USBAudioMicrophone),
        }
    ),
    set_stream_limits,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(
        config[CONF_ID], stream_info(config), config[CONF_BUFFER_SIZE]
    )
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)
    hub = await cg.get_variable(config[CONF_USB_AUDIO_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.register_microphone(var))
