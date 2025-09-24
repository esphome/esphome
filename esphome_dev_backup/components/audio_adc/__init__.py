from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MIC_GAIN
from esphome.core import coroutine_with_priority

CODEOWNERS = ["@kbx81"]
IS_PLATFORM_COMPONENT = True

audio_adc_ns = cg.esphome_ns.namespace("audio_adc")
AudioAdc = audio_adc_ns.class_("AudioAdc")

SetMicGainAction = audio_adc_ns.class_("SetMicGainAction", automation.Action)


SET_MIC_GAIN_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(AudioAdc),
        cv.Required(CONF_MIC_GAIN): cv.templatable(cv.decibel),
    },
    key=CONF_MIC_GAIN,
)


@automation.register_action(
    "audio_adc.set_mic_gain", SetMicGainAction, SET_MIC_GAIN_ACTION_SCHEMA
)
async def audio_adc_set_mic_gain_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config.get(CONF_MIC_GAIN), args, float)
    cg.add(var.set_mic_gain(template_))

    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_AUDIO_ADC")
    cg.add_global(audio_adc_ns.using)
