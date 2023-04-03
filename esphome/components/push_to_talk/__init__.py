import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import binary_sensor, voice_assistant
from esphome.const import CONF_ID, CONF_BINARY_SENSOR

AUTO_LOAD = ["voice_assistant"]
DEPENDENCIES = ["microphone", "binary_sensor"]

push_to_talk_ns = cg.esphome_ns.namespace("push_to_talk")
PushToTalk = push_to_talk_ns.class_(
    "PushToTalk", cg.Component, voice_assistant.VoiceAssistant
)


CONFIG_SCHEMA = voice_assistant.VOICE_ASSISTANT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(PushToTalk),
        cv.Required(CONF_BINARY_SENSOR): cv.use_id(binary_sensor.BinarySensor),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await voice_assistant.setup_voice_assistant(var, config)

    binary_sensor_ = await cg.get_variable(config[CONF_BINARY_SENSOR])
    cg.add(var.set_binary_sensor(binary_sensor_))
