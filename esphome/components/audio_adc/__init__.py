from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_MIC_GAIN
from esphome.core import CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
IS_PLATFORM_COMPONENT = True

audio_adc_ns = cg.esphome_ns.namespace("audio_adc")
AudioAdc = audio_adc_ns.class_("AudioAdc")


SET_MIC_GAIN_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(AudioAdc),
        cv.Required(CONF_MIC_GAIN): cv.templatable(cv.decibel),
    },
    key=CONF_MIC_GAIN,
)


automation.register_apply_action(
    "audio_adc.set_mic_gain",
    SET_MIC_GAIN_ACTION_SCHEMA,
    automation.ApplyField(CONF_MIC_GAIN, "set_mic_gain", cg.float_),
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_AUDIO_ADC")
    cg.add_global(audio_adc_ns.using)
