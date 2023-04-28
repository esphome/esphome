from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.automation import maybe_simple_id
from esphome.const import CONF_ID, CONF_DATA
from esphome.core import CORE
from esphome.coroutine import coroutine_with_priority


CODEOWNERS = ["@jesserockz"]

IS_PLATFORM_COMPONENT = True

speaker_ns = cg.esphome_ns.namespace("speaker")

Speaker = speaker_ns.class_("Speaker")

PlayAction = speaker_ns.class_(
    "PlayAction", automation.Action, cg.Parented.template(Speaker)
)
StopAction = speaker_ns.class_(
    "StopAction", automation.Action, cg.Parented.template(Speaker)
)

IsPlayingCondition = speaker_ns.class_("IsPlayingCondition", automation.Condition)


async def setup_speaker_core_(var, config):
    pass


async def register_speaker(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_speaker_core_(var, config)


SPEAKER_SCHEMA = cv.Schema({})


SPEAKER_AUTOMATION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(Speaker)})


async def speaker_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


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
)
async def speaker_play_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var


automation.register_action("speaker.stop", StopAction, SPEAKER_AUTOMATION_SCHEMA)(
    speaker_action
)

automation.register_condition(
    "speaker.is_playing", IsPlayingCondition, SPEAKER_AUTOMATION_SCHEMA
)(speaker_action)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(speaker_ns.using)
    cg.add_define("USE_SPEAKER")
