from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, media_player, media_source, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
)
from esphome.core.entity_helpers import inherit_property_from
from esphome.types import ConfigType

AUTO_LOAD = ["audio"]
DEPENDENCIES = ["media_source", "speaker"]

CODEOWNERS = ["@kahrendt"]

CONF_MEDIA_PIPELINE = "media_pipeline"
CONF_ON_MUTE = "on_mute"
CONF_ON_UNMUTE = "on_unmute"
CONF_ON_VOLUME = "on_volume"
CONF_SOURCES = "sources"
CONF_VOLUME_INCREMENT = "volume_increment"
CONF_VOLUME_INITIAL = "volume_initial"
CONF_VOLUME_MAX = "volume_max"
CONF_VOLUME_MIN = "volume_min"

speaker_source_ns = cg.esphome_ns.namespace("speaker_source")

SpeakerSourceMediaPlayer = speaker_source_ns.class_(
    "SpeakerSourceMediaPlayer", cg.Component, media_player.MediaPlayer
)

PipelineContext = speaker_source_ns.struct("PipelineContext")

Pipeline = speaker_source_ns.enum("Pipeline")


FORMAT_MAPPING = {
    "FLAC": "flac",
    "MP3": "mp3",
    "OPUS": "opus",
    "WAV": "wav",
}


# Returns a media_player.MediaPlayerSupportedFormat struct with the configured
# format, sample rate, number of channels, purpose, and bytes per sample
def _get_supported_format_struct(pipeline: ConfigType):
    args = [
        media_player.MediaPlayerSupportedFormat,
    ]

    args.append(("format", FORMAT_MAPPING[pipeline[CONF_FORMAT]]))

    args.append(("sample_rate", pipeline[CONF_SAMPLE_RATE]))
    args.append(("num_channels", pipeline[CONF_NUM_CHANNELS]))
    args.append(("purpose", media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["default"]))

    # Omit sample_bytes for MP3: ffmpeg transcoding in Home Assistant fails
    # if the number of bytes per sample is specified for MP3.
    if pipeline[CONF_FORMAT] != "MP3":
        args.append(("sample_bytes", 2))

    return cg.StructInitializer(*args)


def _validate_pipeline(config: ConfigType) -> ConfigType:
    # Inherit settings from speaker if not manually set
    inherit_property_from(CONF_NUM_CHANNELS, CONF_SPEAKER)(config)
    inherit_property_from(CONF_SAMPLE_RATE, CONF_SPEAKER)(config)

    # Opus only supports 48 kHz
    if config.get(CONF_FORMAT) == "OPUS" and config.get(CONF_SAMPLE_RATE) != 48000:
        raise cv.Invalid("Opus only supports a sample rate of 48000 Hz")

    audio.final_validate_audio_schema(
        "speaker_source media_player",
        audio_device=CONF_SPEAKER,
        bits_per_sample=16,
        channels=config.get(CONF_NUM_CHANNELS),
        sample_rate=config.get(CONF_SAMPLE_RATE),
    )(config)

    return config


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
            cv.Required(CONF_MEDIA_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_ON_MUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UNMUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_VOLUME): automation.validate_automation(single=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(media_player.media_player_schema(SpeakerSourceMediaPlayer)),
    cv.only_on_esp32,
    _validate_volume_settings,
)


def _final_validate_codecs(config: ConfigType) -> ConfigType:
    pipeline = config[CONF_MEDIA_PIPELINE]
    fmt = pipeline[CONF_FORMAT]
    if fmt == "NONE":
        audio.request_flac_support()
        audio.request_mp3_support()
        audio.request_opus_support()
    elif fmt == "FLAC":
        audio.request_flac_support()
    elif fmt == "MP3":
        audio.request_mp3_support()
    elif fmt == "OPUS":
        audio.request_opus_support()

    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_MEDIA_PIPELINE): _validate_pipeline,
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

    pipeline_config = config[CONF_MEDIA_PIPELINE]
    pipeline_enum = Pipeline.MEDIA_PIPELINE

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
                _get_supported_format_struct(pipeline_config),
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
