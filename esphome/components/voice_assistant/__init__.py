import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_MICROPHONE
from esphome import automation
from esphome.automation import register_action
from esphome.components import microphone

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["api", "microphone"]

CODEOWNERS = ["@jesserockz"]

voice_assistant_ns = cg.esphome_ns.namespace("voice_assistant")
VoiceAssistant = voice_assistant_ns.class_("VoiceAssistant", cg.Component)

StartAction = voice_assistant_ns.class_(
    "StartAction", automation.Action, cg.Parented.template(VoiceAssistant)
)
StopAction = voice_assistant_ns.class_(
    "StopAction", automation.Action, cg.Parented.template(VoiceAssistant)
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VoiceAssistant),
        cv.GenerateID(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic))

    cg.add_define("USE_VOICE_ASSISTANT")


VOICE_ASSISTANT_ACTION_SCHEMA = cv.Schema({cv.GenerateID(): cv.use_id(VoiceAssistant)})


@register_action("voice_assistant.start", StartAction, VOICE_ASSISTANT_ACTION_SCHEMA)
async def voice_assistant_listen_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@register_action("voice_assistant.stop", StopAction, VOICE_ASSISTANT_ACTION_SCHEMA)
async def voice_assistant_stop_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
