from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, media_player, media_source, speaker
from esphome.components.const import (
    CONF_VOLUME_INCREMENT,
    CONF_VOLUME_INITIAL,
    CONF_VOLUME_MAX,
    CONF_VOLUME_MIN,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DELAY,
    CONF_FORMAT,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
)
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArgsType
from esphome.types import ConfigType

AUTO_LOAD = ["audio"]
DEPENDENCIES = ["media_source", "speaker"]

CODEOWNERS = ["@kahrendt"]

CONF_ANNOUNCEMENT_PIPELINE = "announcement_pipeline"
CONF_MEDIA_PIPELINE = "media_pipeline"
CONF_ON_MUTE = "on_mute"
CONF_PIPELINE = "pipeline"
CONF_ON_UNMUTE = "on_unmute"
CONF_ON_VOLUME = "on_volume"
CONF_SOURCES = "sources"

speaker_source_ns = cg.esphome_ns.namespace("speaker_source")

SpeakerSourceMediaPlayer = speaker_source_ns.class_(
    "SpeakerSourceMediaPlayer", cg.Component, media_player.MediaPlayer
)

PipelineContext = speaker_source_ns.struct("PipelineContext")

Pipeline = speaker_source_ns.enum("Pipeline")
PIPELINE_ENUM = {
    "media": Pipeline.MEDIA_PIPELINE,
    "announcement": Pipeline.ANNOUNCEMENT_PIPELINE,
}

# Maps config key -> (C++ Pipeline enum value, format purpose)
_PIPELINE_INFO = {
    CONF_MEDIA_PIPELINE: (
        Pipeline.MEDIA_PIPELINE,
        media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["default"],
    ),
    CONF_ANNOUNCEMENT_PIPELINE: (
        Pipeline.ANNOUNCEMENT_PIPELINE,
        media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["announcement"],
    ),
}

SetPlaylistDelayAction = speaker_source_ns.class_(
    "SetPlaylistDelayAction", automation.Action
)


_validate_pipeline = media_player.validate_preferred_format(
    "speaker_source media_player", CONF_SPEAKER
)


PIPELINE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(
            PipelineContext
        ),  # Needed to inherit audio settings from the speaker
        cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        cv.Required(CONF_SOURCES): cv.All(
            cv.ensure_list(cv.use_id(media_source.MediaSource)),
            cv.Length(min=1),
        ),
        cv.Optional(CONF_FORMAT, default="FLAC"): cv.enum(audio.AUDIO_FILE_TYPE_ENUM),
        cv.Optional(CONF_SAMPLE_RATE): cv.int_range(min=1),
        cv.Optional(CONF_NUM_CHANNELS): cv.int_range(1, 2),
    }
)


def _validate_no_shared_resources(config: ConfigType) -> ConfigType:
    announcement_config = config.get(CONF_ANNOUNCEMENT_PIPELINE)
    media_config = config.get(CONF_MEDIA_PIPELINE)

    # Check for duplicates within each pipeline
    for pipeline_key in (CONF_ANNOUNCEMENT_PIPELINE, CONF_MEDIA_PIPELINE):
        if pipeline_config := config.get(pipeline_key):
            source_ids = [s.id for s in pipeline_config[CONF_SOURCES]]
            if len(source_ids) != len(set(source_ids)):
                raise cv.Invalid(
                    f"Duplicate media sources in {pipeline_key}. "
                    "Each media source can only appear once per pipeline."
                )

    # Check for sources shared between pipelines
    if announcement_config and media_config:
        if announcement_config[CONF_SPEAKER] == media_config[CONF_SPEAKER]:
            raise cv.Invalid(
                "The announcement and media pipelines cannot use the same speaker. "
                "Use the `mixer` speaker component to create two source speakers."
            )

        announcement_source_ids = {s.id for s in announcement_config[CONF_SOURCES]}
        media_source_ids = {s.id for s in media_config[CONF_SOURCES]}
        shared = announcement_source_ids & media_source_ids
        if shared:
            raise cv.Invalid(
                f"Media sources cannot be shared between pipelines: {', '.join(shared)}. "
                "Create separate media source instances for each pipeline."
            )

    return config


def _validate_volume_settings(config: ConfigType) -> ConfigType:
    # CONF_VOLUME_INITIAL is in the scaled volume domain (0.0-1.0) and doesn't need to be validated
    if config[CONF_VOLUME_MIN] > config[CONF_VOLUME_MAX]:
        raise cv.Invalid(
            f"{CONF_VOLUME_MIN} ({config[CONF_VOLUME_MIN]}) must be less than or equal to {CONF_VOLUME_MAX} ({config[CONF_VOLUME_MAX]})"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(CONF_VOLUME_INITIAL, default=0.5): cv.percentage,
            cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(CONF_ANNOUNCEMENT_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_MEDIA_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_ON_MUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UNMUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_VOLUME): automation.validate_automation(single=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(media_player.media_player_schema(SpeakerSourceMediaPlayer)),
    cv.only_on_esp32,
    cv.has_at_least_one_key(CONF_ANNOUNCEMENT_PIPELINE, CONF_MEDIA_PIPELINE),
    _validate_no_shared_resources,
    _validate_volume_settings,
)


def _final_validate_codecs(config: ConfigType) -> ConfigType:
    media_player.request_codecs_for_format_configs(
        config, [CONF_ANNOUNCEMENT_PIPELINE, CONF_MEDIA_PIPELINE]
    )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_ANNOUNCEMENT_PIPELINE): _validate_pipeline,
            cv.Optional(CONF_MEDIA_PIPELINE): _validate_pipeline,
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _final_validate_codecs,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)

    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_initial(config[CONF_VOLUME_INITIAL]))
    cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))

    for pipeline_key, (pipeline_enum, purpose) in _PIPELINE_INFO.items():
        if pipeline_config := config.get(pipeline_key):
            for source in pipeline_config[CONF_SOURCES]:
                src = await cg.get_variable(source)
                cg.add(var.add_media_source(pipeline_enum, src))

            cg.add(
                var.set_speaker(
                    pipeline_enum,
                    await cg.get_variable(pipeline_config[CONF_SPEAKER]),
                )
            )
            if pipeline_config[CONF_FORMAT] != "NONE":
                cg.add(
                    var.set_format(
                        pipeline_enum,
                        media_player.build_supported_format_struct(
                            pipeline_config, purpose
                        ),
                    )
                )

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
        cv.Required(CONF_PIPELINE): cv.enum(PIPELINE_ENUM, lower=True),
        cv.Required(CONF_DELAY): cv.templatable(cv.positive_time_period_milliseconds),
    }
)


@automation.register_action(
    "speaker_source.set_playlist_delay",
    SetPlaylistDelayAction,
    SET_PLAYLIST_DELAY_ACTION_SCHEMA,
    synchronous=True,
)
async def set_playlist_delay_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    template_ = await cg.templatable(config[CONF_PIPELINE], args, cg.uint8)
    cg.add(var.set_pipeline(template_))

    template_ = await cg.templatable(config[CONF_DELAY], args, cg.uint32)
    cg.add(var.set_delay(template_))

    return var
