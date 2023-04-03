import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_MICROPHONE
from esphome.components import microphone

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["api", "microphone"]

CODEOWNERS = ["@jesserockz"]

voice_assistant_ns = cg.esphome_ns.namespace("voice_assistant")
VoiceAssistant = voice_assistant_ns.class_("VoiceAssistant")

VOICE_ASSISTANT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
    }
)


async def setup_voice_assistant(var, config):
    mic = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic))


async def to_code(config):
    cg.add_define("USE_VOICE_ASSISTANT")
