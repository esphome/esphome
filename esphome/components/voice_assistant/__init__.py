import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_MICROPHONE, CONF_SPEAKER
from esphome import automation
from esphome.automation import register_action
from esphome.components import microphone, speaker

AUTO_LOAD = ["socket"]
DEPENDENCIES = ["api", "microphone"]

CODEOWNERS = ["@jesserockz"]

CONF_ON_START = "on_start"
CONF_ON_STT_END = "on_stt_end"
CONF_ON_TTS_START = "on_tts_start"
CONF_ON_TTS_END = "on_tts_end"
CONF_ON_END = "on_end"
CONF_ON_ERROR = "on_error"


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
        cv.Optional(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        cv.Optional(CONF_ON_START): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_STT_END): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_TTS_START): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_TTS_END): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_END): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic))

    if CONF_SPEAKER in config:
        spkr = await cg.get_variable(config[CONF_SPEAKER])
        cg.add(var.set_speaker(spkr))

    if CONF_ON_START in config:
        await automation.build_automation(
            var.get_start_trigger(), [], config[CONF_ON_START]
        )

    if CONF_ON_STT_END in config:
        await automation.build_automation(
            var.get_stt_end_trigger(), [(cg.std_string, "x")], config[CONF_ON_STT_END]
        )

    if CONF_ON_TTS_START in config:
        await automation.build_automation(
            var.get_tts_start_trigger(),
            [(cg.std_string, "x")],
            config[CONF_ON_TTS_START],
        )

    if CONF_ON_TTS_END in config:
        await automation.build_automation(
            var.get_tts_end_trigger(), [(cg.std_string, "x")], config[CONF_ON_TTS_END]
        )

    if CONF_ON_END in config:
        await automation.build_automation(
            var.get_end_trigger(), [], config[CONF_ON_END]
        )

    if CONF_ON_ERROR in config:
        await automation.build_automation(
            var.get_error_trigger(),
            [(cg.std_string, "code"), (cg.std_string, "message")],
            config[CONF_ON_ERROR],
        )

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
