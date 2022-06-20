from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.automation import maybe_simple_id
from esphome.const import CONF_ID, CONF_ON_STATE, CONF_TRIGGER_ID
from esphome.core import CORE
from esphome.coroutine import coroutine_with_priority
from esphome.cpp_helpers import setup_entity


CODEOWNERS = ["@jesserockz"]

IS_PLATFORM_COMPONENT = True

media_player_ns = cg.esphome_ns.namespace("media_player")

MediaPlayer = media_player_ns.class_("MediaPlayer")

PlayAction = media_player_ns.class_(
    "PlayAction", automation.Action, cg.Parented.template(MediaPlayer)
)
PlayMediaAction = media_player_ns.class_(
    "PlayMediaAction", automation.Action, cg.Parented.template(MediaPlayer)
)
ToggleAction = media_player_ns.class_(
    "ToggleAction", automation.Action, cg.Parented.template(MediaPlayer)
)
PauseAction = media_player_ns.class_(
    "PauseAction", automation.Action, cg.Parented.template(MediaPlayer)
)
StopAction = media_player_ns.class_(
    "StopAction", automation.Action, cg.Parented.template(MediaPlayer)
)
VolumeUpAction = media_player_ns.class_(
    "VolumeUpAction", automation.Action, cg.Parented.template(MediaPlayer)
)
VolumeDownAction = media_player_ns.class_(
    "VolumeDownAction", automation.Action, cg.Parented.template(MediaPlayer)
)
VolumeSetAction = media_player_ns.class_(
    "VolumeSetAction", automation.Action, cg.Parented.template(MediaPlayer)
)


CONF_VOLUME = "volume"
CONF_ON_IDLE = "on_idle"
CONF_ON_PLAY = "on_play"
CONF_ON_PAUSE = "on_pause"
CONF_MEDIA_URL = "media_url"

StateTrigger = media_player_ns.class_("StateTrigger", automation.Trigger.template())
IdleTrigger = media_player_ns.class_("IdleTrigger", automation.Trigger.template())
PlayTrigger = media_player_ns.class_("PlayTrigger", automation.Trigger.template())
PauseTrigger = media_player_ns.class_("PauseTrigger", automation.Trigger.template())
IsIdleCondition = media_player_ns.class_("IsIdleCondition", automation.Condition)
IsPlayingCondition = media_player_ns.class_("IsPlayingCondition", automation.Condition)


async def setup_media_player_core_(var, config):
    await setup_entity(var, config)
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_IDLE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PLAY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PAUSE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_media_player(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_media_player(var))
    await setup_media_player_core_(var, config)


MEDIA_PLAYER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_ON_STATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StateTrigger),
            }
        ),
        cv.Optional(CONF_ON_IDLE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IdleTrigger),
            }
        ),
        cv.Optional(CONF_ON_PLAY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PlayTrigger),
            }
        ),
        cv.Optional(CONF_ON_PAUSE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PauseTrigger),
            }
        ),
    }
)


MEDIA_PLAYER_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(MediaPlayer)})


@automation.register_action(
    "media_player.play_media",
    PlayMediaAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(MediaPlayer),
            cv.Required(CONF_MEDIA_URL): cv.templatable(cv.url),
        },
        key=CONF_MEDIA_URL,
    ),
)
async def media_player_play_media_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    media_url = await cg.templatable(config[CONF_MEDIA_URL], args, cg.std_string)
    cg.add(var.set_media_url(media_url))
    return var


@automation.register_action("media_player.play", PlayAction, MEDIA_PLAYER_ACTION_SCHEMA)
@automation.register_action(
    "media_player.toggle", ToggleAction, MEDIA_PLAYER_ACTION_SCHEMA
)
@automation.register_action(
    "media_player.pause", PauseAction, MEDIA_PLAYER_ACTION_SCHEMA
)
@automation.register_action("media_player.stop", StopAction, MEDIA_PLAYER_ACTION_SCHEMA)
@automation.register_action(
    "media_player.volume_up", VolumeUpAction, MEDIA_PLAYER_ACTION_SCHEMA
)
@automation.register_action(
    "media_player.volume_down", VolumeDownAction, MEDIA_PLAYER_ACTION_SCHEMA
)
@automation.register_condition(
    "media_player.is_idle", IsIdleCondition, MEDIA_PLAYER_ACTION_SCHEMA
)
@automation.register_condition(
    "media_player.is_playing", IsPlayingCondition, MEDIA_PLAYER_ACTION_SCHEMA
)
async def media_player_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "media_player.volume_set",
    VolumeSetAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(MediaPlayer),
            cv.Required(CONF_VOLUME): cv.templatable(cv.percentage),
        },
        key=CONF_VOLUME,
    ),
)
async def media_player_volume_set_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    volume = await cg.templatable(config[CONF_VOLUME], args, float)
    cg.add(var.set_volume(volume))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(media_player_ns.using)
    cg.add_define("USE_MEDIA_PLAYER")
