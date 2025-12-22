from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, media_player, media_source, socket, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_DELAY,
    CONF_FORMAT,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
)
from esphome.core.entity_helpers import inherit_property_from

from . import speaker_source_ns

CONF_ANNOUNCEMENT_PIPELINE = "announcement_pipeline"
CONF_ANNOUNCEMENT_SPEAKER = "announcement_speaker"
CONF_MEDIA_PIPELINE = "media_pipeline"
CONF_MEDIA_SPEAKER = "media_speaker"
CONF_ON_MUTE = "on_mute"
CONF_ON_UNMUTE = "on_unmute"
CONF_ON_VOLUME = "on_volume"
CONF_PIPELINE = "pipeline"
CONF_PLAYLIST_DELAY = "playlist_delay"
CONF_SOURCES = "sources"
CONF_TASK_STACK_IN_PSRAM = "task_stack_in_psram"
CONF_VOLUME_INCREMENT = "volume_increment"
CONF_VOLUME_INITIAL = "volume_initial"
CONF_VOLUME_MAX = "volume_max"
CONF_VOLUME_MIN = "volume_min"

SpeakerSourceMediaPlayer = speaker_source_ns.class_(
    "SpeakerSourceMediaPlayer", cg.Component, media_player.MediaPlayer
)

MuteTrigger = speaker_source_ns.class_("MuteTrigger", automation.Trigger.template())
UnmuteTrigger = speaker_source_ns.class_("UnmuteTrigger", automation.Trigger.template())
VolumeTrigger = speaker_source_ns.class_(
    "VolumeTrigger", automation.Trigger.template(cg.float_)
)

SetPlaylistDelayAction = speaker_source_ns.class_(
    "SetPlaylistDelayAction", automation.Action
)


# Returns a media_player.MediaPlayerSupportedFormat struct with the configured
# format, sample rate, number of channels, purpose, and bytes per sample
def _get_supported_format_struct(pipeline, pipeline_type):
    args = [
        media_player.MediaPlayerSupportedFormat,
    ]

    if pipeline[CONF_FORMAT] == "FLAC":
        args.append(("format", "flac"))
    elif pipeline[CONF_FORMAT] == "MP3":
        args.append(("format", "mp3"))
    elif pipeline[CONF_FORMAT] == "WAV":
        args.append(("format", "wav"))

    args.append(("sample_rate", pipeline[CONF_SAMPLE_RATE]))
    args.append(("num_channels", pipeline[CONF_NUM_CHANNELS]))

    if pipeline_type == "MEDIA":
        args.append(
            (
                "purpose",
                media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["default"],
            )
        )
    elif pipeline_type == "ANNOUNCEMENT":
        args.append(
            (
                "purpose",
                media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["announcement"],
            )
        )
    if pipeline[CONF_FORMAT] != "MP3":
        args.append(("sample_bytes", 2))

    return cg.StructInitializer(*args)


def _validate_pipeline(config):
    # Inherit settings from speaker if not manually set
    if CONF_ANNOUNCEMENT_SPEAKER in config:
        inherit_property_from(CONF_NUM_CHANNELS, CONF_ANNOUNCEMENT_SPEAKER)(config)
        inherit_property_from(CONF_SAMPLE_RATE, CONF_ANNOUNCEMENT_SPEAKER)(config)
        # TODO: Probably should be handled by http_request media_source
        socket.consume_sockets(1, "speaker_source_announcement")(config)
    elif CONF_MEDIA_SPEAKER in config:
        inherit_property_from(CONF_NUM_CHANNELS, CONF_MEDIA_SPEAKER)(config)
        inherit_property_from(CONF_SAMPLE_RATE, CONF_MEDIA_SPEAKER)(config)
        socket.consume_sockets(1, "speaker_source_media")(config)

    # Validate the settings are compatible with the speakers
    if CONF_ANNOUNCEMENT_SPEAKER in config:
        audio.final_validate_audio_schema(
            "speaker_source media_player announcement",
            audio_device=CONF_ANNOUNCEMENT_SPEAKER,
            bits_per_sample=16,
            channels=config.get(CONF_NUM_CHANNELS),
            sample_rate=config.get(CONF_SAMPLE_RATE),
        )(config)
    if CONF_MEDIA_SPEAKER in config:
        audio.final_validate_audio_schema(
            "speaker_source media_player media",
            audio_device=CONF_MEDIA_SPEAKER,
            bits_per_sample=16,
            channels=config.get(CONF_NUM_CHANNELS),
            sample_rate=config.get(CONF_SAMPLE_RATE),
        )(config)

    return config


PIPELINE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FORMAT, default="FLAC"): cv.enum(audio.AUDIO_FILE_TYPE_ENUM),
        cv.Optional(CONF_SAMPLE_RATE): cv.int_range(min=1),
        cv.Optional(CONF_NUM_CHANNELS): cv.int_range(1, 2),
        cv.Optional(
            CONF_PLAYLIST_DELAY, default="0ms"
        ): cv.positive_time_period_milliseconds,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SpeakerSourceMediaPlayer),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(CONF_VOLUME_INITIAL, default=0.5): cv.percentage,
            cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(CONF_SOURCES): cv.ensure_list(
                cv.use_id(media_source.MediaSource)
            ),
            cv.Optional(CONF_ANNOUNCEMENT_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_MEDIA_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Optional(CONF_ANNOUNCEMENT_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_MEDIA_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_ON_MUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UNMUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_VOLUME): automation.validate_automation(single=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(media_player.media_player_schema(SpeakerSourceMediaPlayer))
)

FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ANNOUNCEMENT_PIPELINE): _validate_pipeline,
        cv.Optional(CONF_MEDIA_PIPELINE): _validate_pipeline,
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)

    cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_initial(config[CONF_VOLUME_INITIAL]))
    cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))

    # TODO: Make this configurable... also the file media source may add files with extensions, we should do some validation
    cg.add_define("USE_AUDIO_FLAC_SUPPORT", True)
    cg.add_define("USE_AUDIO_MP3_SUPPORT", True)

    if CONF_SOURCES in config:
        for source in config[CONF_SOURCES]:
            src = await cg.get_variable(source)
            cg.add(var.add_media_source(src))

    if CONF_ANNOUNCEMENT_SPEAKER in config:
        spk = await cg.get_variable(config[CONF_ANNOUNCEMENT_SPEAKER])
        cg.add(var.set_announcement_speaker(spk))

    if CONF_MEDIA_SPEAKER in config:
        spk = await cg.get_variable(config[CONF_MEDIA_SPEAKER])
        cg.add(var.set_media_speaker(spk))

    if announcement_pipeline_config := config.get(CONF_ANNOUNCEMENT_PIPELINE):
        cg.add(
            var.set_announcement_format(
                _get_supported_format_struct(
                    announcement_pipeline_config, "ANNOUNCEMENT"
                )
            )
        )
        cg.add(
            var.set_playlist_delay_ms(
                1, announcement_pipeline_config[CONF_PLAYLIST_DELAY]
            )
        )

    if media_pipeline_config := config.get(CONF_MEDIA_PIPELINE):
        cg.add(
            var.set_media_format(
                _get_supported_format_struct(media_pipeline_config, "MEDIA")
            )
        )
        cg.add(var.set_playlist_delay_ms(0, media_pipeline_config[CONF_PLAYLIST_DELAY]))

    if on_mute := config.get(CONF_ON_MUTE):
        await automation.build_automation(
            var.get_mute_trigger(),
            [],
            on_mute,
        )
    if on_unmute := config.get(CONF_ON_UNMUTE):
        await automation.build_automation(
            var.get_unmute_trigger(),
            [],
            on_unmute,
        )
    if on_volume := config.get(CONF_ON_VOLUME):
        await automation.build_automation(
            var.get_volume_trigger(),
            [(cg.float_, "x")],
            on_volume,
        )


SET_PLAYLIST_DELAY_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(SpeakerSourceMediaPlayer),
        cv.Required(CONF_PIPELINE): cv.templatable(cv.uint8_t),
        cv.Required(CONF_DELAY): cv.templatable(cv.positive_time_period_milliseconds),
    }
)


@automation.register_action(
    "speaker_source.set_playlist_delay",
    SetPlaylistDelayAction,
    SET_PLAYLIST_DELAY_ACTION_SCHEMA,
)
async def set_playlist_delay_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    template_ = await cg.templatable(config[CONF_PIPELINE], args, cg.uint8)
    cg.add(var.set_pipeline(template_))

    template_ = await cg.templatable(config[CONF_DELAY], args, cg.uint32)
    cg.add(var.set_delay(template_))

    return var
