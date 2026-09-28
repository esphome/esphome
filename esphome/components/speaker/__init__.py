from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, audio_dac
import esphome.config_validation as cv
from esphome.const import CONF_AUDIO_DAC, CONF_DATA, CONF_ID, CONF_VOLUME
from esphome.core import CORE, ID
from esphome.coroutine import CoroPriority, coroutine_with_priority

AUTO_LOAD = ["audio"]
CODEOWNERS = ["@jesserockz", "@kahrendt"]

IS_PLATFORM_COMPONENT = True

speaker_ns = cg.esphome_ns.namespace("speaker")

Speaker = speaker_ns.class_("Speaker")

PlayAction = speaker_ns.class_(
    "PlayAction", automation.Action, cg.Parented.template(Speaker)
)


async def setup_speaker_core_(var, config):
    if audio_dac_config := config.get(CONF_AUDIO_DAC):
        aud_dac = await cg.get_variable(audio_dac_config)
        cg.add(var.set_audio_dac(aud_dac))


async def register_speaker(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_speaker_core_(var, config)


SPEAKER_SCHEMA = cv.Schema.extend(audio.AUDIO_COMPONENT_SCHEMA).extend(
    {
        cv.Optional(CONF_AUDIO_DAC): cv.use_id(audio_dac.AudioDac),
    }
)

SPEAKER_AUTOMATION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(Speaker)}
)


@automation.register_action(
    "speaker.play",
    PlayAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(Speaker),
            cv.Required(CONF_DATA): cv.templatable(cv.ensure_list(cv.hex_uint8_t)),
        },
        key=CONF_DATA,
    ),
    synchronous=True,
)
async def speaker_play_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        # Generate static array in flash to avoid RAM copy
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
        cg.add(var.set_data_static(arr, len(data)))
    return var


for _name, _call in (
    ("speaker.stop", "stop()"),
    ("speaker.finish", "finish()"),
    ("speaker.mute_on", "set_mute_state(true)"),
    ("speaker.mute_off", "set_mute_state(false)"),
):
    automation.register_apply_action(
        _name, SPEAKER_AUTOMATION_SCHEMA, automation.ApplyCall(_call)
    )

automation.register_apply_condition(
    "speaker.is_playing", SPEAKER_AUTOMATION_SCHEMA, "is_running()"
)
automation.register_apply_condition(
    "speaker.is_stopped", SPEAKER_AUTOMATION_SCHEMA, "is_stopped()"
)


automation.register_apply_action(
    "speaker.volume_set",
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(Speaker),
            cv.Required(CONF_VOLUME): cv.templatable(cv.percentage),
        },
        key=CONF_VOLUME,
    ),
    automation.ApplyField(CONF_VOLUME, "set_volume", cg.float_),
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(speaker_ns.using)
    cg.add_define("USE_SPEAKER")
