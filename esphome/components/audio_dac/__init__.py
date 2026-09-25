from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_VOLUME
from esphome.core import CoroPriority, coroutine_with_priority
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
IS_PLATFORM_COMPONENT = True

audio_dac_ns = cg.esphome_ns.namespace("audio_dac")
AudioDac = audio_dac_ns.class_("AudioDac")


MUTE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AudioDac),
    }
)

SET_VOLUME_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(AudioDac),
        cv.Required(CONF_VOLUME): cv.templatable(cv.percentage),
    },
    key=CONF_VOLUME,
)


for _name, _call in (
    ("audio_dac.mute_off", "set_mute_off()"),
    ("audio_dac.mute_on", "set_mute_on()"),
):
    automation.register_apply_action(
        _name, MUTE_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

automation.register_apply_action(
    "audio_dac.set_volume",
    SET_VOLUME_ACTION_SCHEMA,
    automation.ApplyField(CONF_VOLUME, "set_volume", cg.float_),
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_AUDIO_DAC")
    cg.add_global(audio_dac_ns.using)
