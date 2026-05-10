from collections.abc import Callable

from esphome import automation
import esphome.codegen as cg
from esphome.components import audio
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_FORMAT,
    CONF_ICON,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_ON_IDLE,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_SAMPLE_RATE,
    CONF_VOLUME,
)
from esphome.core import CORE
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    inherit_property_from,
    queue_entity_register,
    setup_entity,
)
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@jesserockz"]

IS_PLATFORM_COMPONENT = True

media_player_ns = cg.esphome_ns.namespace("media_player")

MediaPlayer = media_player_ns.class_("MediaPlayer")

MediaPlayerSupportedFormat = media_player_ns.struct("MediaPlayerSupportedFormat")

MediaPlayerFormatPurpose = media_player_ns.enum(
    "MediaPlayerFormatPurpose", is_class=True
)
MEDIA_PLAYER_FORMAT_PURPOSE_ENUM = {
    "default": MediaPlayerFormatPurpose.PURPOSE_DEFAULT,
    "announcement": MediaPlayerFormatPurpose.PURPOSE_ANNOUNCEMENT,
}

# Public API for external components. Do not remove.
FORMAT_MAPPING = {
    "FLAC": "flac",
    "MP3": "mp3",
    "OPUS": "opus",
    "WAV": "wav",
}


def build_supported_format_struct(
    format_config: ConfigType, purpose: MockObj
) -> cg.StructInitializer:
    """Build a MediaPlayerSupportedFormat struct from a format config and purpose.

    Public API for external components. Do not remove.
    """
    args = [
        MediaPlayerSupportedFormat,
        ("format", FORMAT_MAPPING[format_config[CONF_FORMAT]]),
        ("sample_rate", format_config[CONF_SAMPLE_RATE]),
        ("num_channels", format_config[CONF_NUM_CHANNELS]),
        ("purpose", purpose),
    ]

    # Omit sample_bytes for MP3: ffmpeg transcoding in Home Assistant fails
    # if the number of bytes per sample is specified for MP3.
    if format_config[CONF_FORMAT] != "MP3":
        args.append(("sample_bytes", 2))

    return cg.StructInitializer(*args)


def validate_preferred_format(
    component_name: str, audio_device_key: str
) -> Callable[[ConfigType], ConfigType]:
    """Return a validator that inherits audio device settings and validates format constraints.

    Public API for external components. Do not remove.
    """

    def validator(config: ConfigType) -> ConfigType:
        # Inherit settings from audio device if not manually set
        inherit_property_from(CONF_NUM_CHANNELS, audio_device_key)(config)
        inherit_property_from(CONF_SAMPLE_RATE, audio_device_key)(config)

        # Opus only supports 48 kHz
        if config.get(CONF_FORMAT) == "OPUS" and config.get(CONF_SAMPLE_RATE) != 48000:
            raise cv.Invalid("Opus only supports a sample rate of 48000 Hz")

        # Validate the settings are compatible with the audio device
        audio.final_validate_audio_schema(
            component_name,
            audio_device=audio_device_key,
            bits_per_sample=16,
            channels=config.get(CONF_NUM_CHANNELS),
            sample_rate=config.get(CONF_SAMPLE_RATE),
        )(config)

        return config

    return validator


def request_codecs_for_format_configs(
    config: ConfigType, format_config_keys: list[str]
) -> None:
    """Scan format configs for configured formats and request the needed codec support.

    If any config uses "NONE" (accepts any format), all codecs are requested.

    Public API for external components. Do not remove.
    """
    needed_formats: set[str] = set()
    need_all = False

    for key in format_config_keys:
        if format_config := config.get(key):
            fmt = format_config[CONF_FORMAT]
            if fmt == "NONE":
                need_all = True
            else:
                needed_formats.add(fmt)

    if need_all:
        audio.request_flac_support()
        audio.request_mp3_support()
        audio.request_opus_support()
        audio.request_wav_support()
    else:
        if "FLAC" in needed_formats:
            audio.request_flac_support()
        if "MP3" in needed_formats:
            audio.request_mp3_support()
        if "OPUS" in needed_formats:
            audio.request_opus_support()
        if "WAV" in needed_formats:
            audio.request_wav_support()


# Local config key constants
CONF_ANNOUNCEMENT = "announcement"
CONF_ON_PLAY = "on_play"
CONF_ON_PAUSE = "on_pause"
CONF_ON_ANNOUNCEMENT = "on_announcement"
CONF_MEDIA_URL = "media_url"

# Command actions that all share the same schema and codegen handler
_COMMAND_ACTIONS = [
    "play",
    "pause",
    "stop",
    "toggle",
    "volume_up",
    "volume_down",
    "turn_on",
    "turn_off",
    "next",
    "previous",
    "mute",
    "unmute",
    "repeat_off",
    "repeat_one",
    "repeat_all",
    "shuffle",
    "unshuffle",
    "group_join",
    "clear_playlist",
]

StateAnyForwarder = media_player_ns.class_("StateAnyForwarder")
StateEnterForwarder = media_player_ns.class_("StateEnterForwarder")
MediaPlayerState = media_player_ns.enum("MediaPlayerState")

# State triggers: (config_key, state enum or None for any-state)
_STATE_TRIGGERS = (
    (CONF_ON_STATE, None),
    (CONF_ON_IDLE, MediaPlayerState.MEDIA_PLAYER_STATE_IDLE),
    (CONF_ON_PLAY, MediaPlayerState.MEDIA_PLAYER_STATE_PLAYING),
    (CONF_ON_PAUSE, MediaPlayerState.MEDIA_PLAYER_STATE_PAUSED),
    (CONF_ON_ANNOUNCEMENT, MediaPlayerState.MEDIA_PLAYER_STATE_ANNOUNCING),
    (CONF_ON_TURN_ON, MediaPlayerState.MEDIA_PLAYER_STATE_ON),
    (CONF_ON_TURN_OFF, MediaPlayerState.MEDIA_PLAYER_STATE_OFF),
)

# State conditions that all share the same schema and codegen handler
_STATE_CONDITIONS = [
    "idle",
    "paused",
    "playing",
    "announcing",
    "on",
    "off",
    "muted",
]

# Special action classes with custom schemas/handlers
PlayMediaAction = media_player_ns.class_(
    "PlayMediaAction", automation.Action, cg.Parented.template(MediaPlayer)
)
EnqueueMediaAction = media_player_ns.class_(
    "EnqueueMediaAction", automation.Action, cg.Parented.template(MediaPlayer)
)
VolumeSetAction = media_player_ns.class_(
    "VolumeSetAction", automation.Action, cg.Parented.template(MediaPlayer)
)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_STATE, "add_on_state_callback", forwarder=StateAnyForwarder
    ),
    automation.CallbackAutomation(
        CONF_ON_IDLE,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            MediaPlayerState.MEDIA_PLAYER_STATE_IDLE
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_PLAY,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            MediaPlayerState.MEDIA_PLAYER_STATE_PLAYING
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_PAUSE,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            MediaPlayerState.MEDIA_PLAYER_STATE_PAUSED
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_ANNOUNCEMENT,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            MediaPlayerState.MEDIA_PLAYER_STATE_ANNOUNCING
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_TURN_ON,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(MediaPlayerState.MEDIA_PLAYER_STATE_ON),
    ),
    automation.CallbackAutomation(
        CONF_ON_TURN_OFF,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(MediaPlayerState.MEDIA_PLAYER_STATE_OFF),
    ),
)


@setup_entity("media_player")
async def setup_media_player_core_(var, config):
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


async def register_media_player(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("media_player", config)
    CORE.register_platform_component("media_player", var)
    await setup_media_player_core_(var, config)


async def new_media_player(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_media_player(var, config)
    return var


_MEDIA_PLAYER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.Optional(conf_key): automation.validate_automation({})
        for conf_key, _ in _STATE_TRIGGERS
    }
)

_MEDIA_PLAYER_SCHEMA.add_extra(entity_duplicate_validator("media_player"))


def media_player_schema(
    class_: MockObjClass,
    *,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {cv.GenerateID(CONF_ID): cv.declare_id(class_)}

    for key, default, validator in [
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _MEDIA_PLAYER_SCHEMA.extend(schema)


MEDIA_PLAYER_ACTION_SCHEMA = automation.maybe_simple_id(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(MediaPlayer),
            cv.Optional(CONF_ANNOUNCEMENT, default=False): cv.templatable(cv.boolean),
        }
    )
)

MEDIA_PLAYER_CONDITION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(MediaPlayer)}
)


_MEDIA_URL_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(MediaPlayer),
        cv.Required(CONF_MEDIA_URL): cv.templatable(cv.url),
        cv.Optional(CONF_ANNOUNCEMENT, default=False): cv.templatable(cv.boolean),
    },
    key=CONF_MEDIA_URL,
)


async def _media_action_handler(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    media_url = await cg.templatable(config[CONF_MEDIA_URL], args, cg.std_string)
    announcement = await cg.templatable(config[CONF_ANNOUNCEMENT], args, cg.bool_)
    cg.add(var.set_media_url(media_url))
    cg.add(var.set_announcement(announcement))
    return var


automation.register_action(
    "media_player.play_media",
    PlayMediaAction,
    _MEDIA_URL_ACTION_SCHEMA,
    synchronous=True,
)(_media_action_handler)

automation.register_action(
    "media_player.enqueue",
    EnqueueMediaAction,
    _MEDIA_URL_ACTION_SCHEMA,
    synchronous=True,
)(_media_action_handler)


def _snake_to_camel(name):
    return "".join(word.capitalize() for word in name.split("_"))


def _register_command_actions():
    async def handler(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg)
        await cg.register_parented(var, config[CONF_ID])
        announcement = await cg.templatable(config[CONF_ANNOUNCEMENT], args, cg.bool_)
        cg.add(var.set_announcement(announcement))
        return var

    for action_name in _COMMAND_ACTIONS:
        class_name = f"{_snake_to_camel(action_name)}Action"
        action_class = media_player_ns.class_(
            class_name, automation.Action, cg.Parented.template(MediaPlayer)
        )
        automation.register_action(
            f"media_player.{action_name}",
            action_class,
            MEDIA_PLAYER_ACTION_SCHEMA,
            synchronous=True,
        )(handler)


_register_command_actions()


def _register_state_conditions():
    async def handler(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg)
        await cg.register_parented(var, config[CONF_ID])
        return var

    for condition_name in _STATE_CONDITIONS:
        class_name = f"Is{_snake_to_camel(condition_name)}Condition"
        condition_class = media_player_ns.class_(class_name, automation.Condition)
        automation.register_condition(
            f"media_player.is_{condition_name}",
            condition_class,
            MEDIA_PLAYER_CONDITION_SCHEMA,
        )(handler)


_register_state_conditions()


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
    synchronous=True,
)
async def media_player_volume_set_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    volume = await cg.templatable(config[CONF_VOLUME], args, cg.float_)
    cg.add(var.set_volume(volume))
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(media_player_ns.using)
    cg.add_define("USE_MEDIA_PLAYER")
