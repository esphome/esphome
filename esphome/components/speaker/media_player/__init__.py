"""Speaker Media Player Setup."""

import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import (
    audio,
    audio_file,
    esp32,
    media_player,
    network,
    ota,
    psram,
    speaker,
)
from esphome.components.const import (
    CONF_VOLUME_INCREMENT,
    CONF_VOLUME_INITIAL,
    CONF_VOLUME_MAX,
    CONF_VOLUME_MIN,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_FILES,
    CONF_FORMAT,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
    CONF_TASK_STACK_IN_PSRAM,
)

_LOGGER = logging.getLogger(__name__)


AUTO_LOAD = ["audio"]
DEPENDENCIES = ["network"]

CODEOWNERS = ["@kahrendt", "@synesthesiam"]
DOMAIN = "media_player"

CONF_ANNOUNCEMENT = "announcement"
CONF_ANNOUNCEMENT_PIPELINE = "announcement_pipeline"
CONF_CODEC_SUPPORT_ENABLED = "codec_support_enabled"  # Remove before 2026.10.0
CONF_ENQUEUE = "enqueue"
CONF_MEDIA_FILE = "media_file"
CONF_MEDIA_PIPELINE = "media_pipeline"
CONF_ON_MUTE = "on_mute"
CONF_ON_UNMUTE = "on_unmute"
CONF_ON_VOLUME = "on_volume"
CONF_STREAM = "stream"


speaker_ns = cg.esphome_ns.namespace("speaker")
SpeakerMediaPlayer = speaker_ns.class_(
    "SpeakerMediaPlayer",
    media_player.MediaPlayer,
    cg.Component,
)

AudioPipeline = speaker_ns.class_("AudioPipeline")
AudioPipelineType = speaker_ns.enum("AudioPipelineType", is_class=True)
AUDIO_PIPELINE_TYPE_ENUM = {
    "MEDIA": AudioPipelineType.MEDIA,
    "ANNOUNCEMENT": AudioPipelineType.ANNOUNCEMENT,
}

PlayOnDeviceMediaAction = speaker_ns.class_(
    "PlayOnDeviceMediaAction",
    automation.Action,
    cg.Parented.template(SpeakerMediaPlayer),
)
StopStreamAction = speaker_ns.class_(
    "StopStreamAction", automation.Action, cg.Parented.template(SpeakerMediaPlayer)
)


_PURPOSE_MAP = {
    "MEDIA": media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["default"],
    "ANNOUNCEMENT": media_player.MEDIA_PLAYER_FORMAT_PURPOSE_ENUM["announcement"],
}


_validate_pipeline = media_player.validate_preferred_format(
    "speaker media_player", CONF_SPEAKER
)


def _validate_repeated_speaker(config):
    if (
        (announcement_config := config.get(CONF_ANNOUNCEMENT_PIPELINE))
        and (media_config := config.get(CONF_MEDIA_PIPELINE))
        and announcement_config[CONF_SPEAKER] == media_config[CONF_SPEAKER]
    ):
        raise cv.Invalid(
            "The announcement and media pipelines cannot use the same speaker. Use the `mixer` speaker component to create two source speakers."
        )

    return config


def _final_validate(config):
    # Remove before 2026.10.0
    if CONF_CODEC_SUPPORT_ENABLED in config:
        _LOGGER.warning(
            "'%s' is deprecated and will be removed in 2026.10.0. "
            "Codec support is now automatically determined from the pipeline "
            "'format' setting. Set format to 'NONE' to enable all codecs.",
            CONF_CODEC_SUPPORT_ENABLED,
        )

    # Request codecs based on pipeline formats. Codecs needed by local files are
    # already requested during CONFIG_SCHEMA validation (via audio_files_schema).
    media_player.request_codecs_for_format_configs(
        config, [CONF_ANNOUNCEMENT_PIPELINE, CONF_MEDIA_PIPELINE]
    )

    return config


PIPELINE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(AudioPipeline),
        cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
        cv.Optional(CONF_FORMAT, default="FLAC"): cv.enum(audio.AUDIO_FILE_TYPE_ENUM),
        cv.Optional(CONF_SAMPLE_RATE): cv.int_range(min=1),
        cv.Optional(CONF_NUM_CHANNELS): cv.int_range(1, 2),
    }
)


def _request_high_performance_networking(config):
    """Request high performance networking for streaming media.

    Speaker media player streams audio data, so it always benefits from
    optimized WiFi and lwip settings regardless of codec support.
    Called during config validation to ensure flags are set before to_code().
    """
    network.require_high_performance_networking()
    return config


CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(SpeakerMediaPlayer).extend(
        {
            cv.Required(CONF_ANNOUNCEMENT_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_MEDIA_PIPELINE): PIPELINE_SCHEMA,
            cv.Optional(CONF_BUFFER_SIZE, default=1000000): cv.int_range(
                min=4000, max=4000000
            ),
            # Remove before 2026.10.0
            cv.Optional(CONF_CODEC_SUPPORT_ENABLED): cv.Any(cv.boolean, cv.string),
            cv.Optional(CONF_FILES): audio_file.audio_files_schema(),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM): cv.All(
                cv.boolean, cv.requires_component(psram.DOMAIN)
            ),
            cv.Optional(CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(CONF_VOLUME_INITIAL, default=0.5): cv.percentage,
            cv.Optional(CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(CONF_ON_MUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_UNMUTE): automation.validate_automation(single=True),
            cv.Optional(CONF_ON_VOLUME): automation.validate_automation(single=True),
        }
    ),
    cv.only_on_esp32,
    _validate_repeated_speaker,
    _request_high_performance_networking,
)


FINAL_VALIDATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_ANNOUNCEMENT_PIPELINE): _validate_pipeline,
            cv.Optional(CONF_MEDIA_PIPELINE): _validate_pipeline,
        },
        extra=cv.ALLOW_EXTRA,
    ),
    _final_validate,
)


async def to_code(config):
    if CONF_ON_TURN_OFF in config or CONF_ON_TURN_ON in config:
        cg.add_define("USE_SPEAKER_MEDIA_PLAYER_ON_OFF", True)

    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)

    ota.request_ota_state_listeners()

    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))

    if config.get(CONF_TASK_STACK_IN_PSRAM):
        cg.add(var.set_task_stack_in_psram(True))
        esp32.add_idf_sdkconfig_option(
            "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
        )

    cg.add(var.set_volume_increment(config[CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_initial(config[CONF_VOLUME_INITIAL]))
    cg.add(var.set_volume_max(config[CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[CONF_VOLUME_MIN]))

    announcement_pipeline_config = config[CONF_ANNOUNCEMENT_PIPELINE]
    spkr = await cg.get_variable(announcement_pipeline_config[CONF_SPEAKER])
    cg.add(var.set_announcement_speaker(spkr))
    if announcement_pipeline_config[CONF_FORMAT] != "NONE":
        cg.add(
            var.set_announcement_format(
                media_player.build_supported_format_struct(
                    announcement_pipeline_config, _PURPOSE_MAP["ANNOUNCEMENT"]
                )
            )
        )

    if media_pipeline_config := config.get(CONF_MEDIA_PIPELINE):
        spkr = await cg.get_variable(media_pipeline_config[CONF_SPEAKER])
        cg.add(var.set_media_speaker(spkr))
        if media_pipeline_config[CONF_FORMAT] != "NONE":
            cg.add(
                var.set_media_format(
                    media_player.build_supported_format_struct(
                        media_pipeline_config, _PURPOSE_MAP["MEDIA"]
                    )
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

    for file_config in config.get(CONF_FILES, []):
        audio_file.generate_audio_file_code(file_config)


@automation.register_action(
    "media_player.speaker.play_on_device_media_file",
    PlayOnDeviceMediaAction,
    cv.maybe_simple_value(
        {
            cv.GenerateID(): cv.use_id(SpeakerMediaPlayer),
            cv.Required(CONF_MEDIA_FILE): cv.use_id(audio.AudioFile),
            cv.Optional(CONF_ANNOUNCEMENT, default=False): cv.templatable(cv.boolean),
            cv.Optional(CONF_ENQUEUE, default=False): cv.templatable(cv.boolean),
        },
        key=CONF_MEDIA_FILE,
    ),
    synchronous=True,
)
async def play_on_device_media_media_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    media_file = await cg.get_variable(config[CONF_MEDIA_FILE])
    announcement = await cg.templatable(config[CONF_ANNOUNCEMENT], args, cg.bool_)
    enqueue = await cg.templatable(config[CONF_ENQUEUE], args, cg.bool_)

    template_ = await cg.templatable(media_file, args, audio.AudioFile.operator("ptr"))
    cg.add(var.set_audio_file(template_))
    cg.add(var.set_announcement(announcement))
    cg.add(var.set_enqueue(enqueue))
    return var
